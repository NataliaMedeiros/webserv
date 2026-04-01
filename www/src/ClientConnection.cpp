#include "ClientConnection.hpp"
#include <sys/socket.h>
#include <cerrno>
#include <iostream>

// ClientConnection represents one open TCP connection with one browser.
// It is a state machine - always in exactly one of three states:
//
//   Reading  -> waiting for and receiving bytes from the browser
//   Writing  -> a response is ready and we are sending it back
//   Closing  -> we are done, EventLoop will remove this connection
//
// State transitions:
//   Reading -> Writing  : request fully received and parsed, response queued
//   Writing -> Reading  : response fully sent, keep-alive is active
//   Writing -> Closing  : response fully sent, connection: close
//   Reading -> Closing  : browser disconnected or sent a bad request

ClientConnection::ClientConnection(int fd) : _fd(fd)
{
    // Start in Reading state - we always wait for the browser to speak first
}

// wantedEvents() tells the EventLoop which poll() events we care about right now.
// This changes depending on our current state.
short ClientConnection::wantedEvents() const
{
    short events = 0;

    // Always listen for incoming data, unless we are already closing
    if (_state != State::Closing)
        events |= POLLIN;

    // Only watch for "writable" if we actually have data waiting to be sent
    if (!_out.empty())
        events |= POLLOUT;

    return events;
}

// shouldRemove() returns true when the EventLoop can safely delete this connection.
// We wait until both conditions are true: state is Closing AND output buffer is empty.
// This ensures we never drop the connection before finishing sending the response.
bool ClientConnection::shouldRemove() const
{
    return (_state == State::Closing) && _out.empty();
}

// onReadable() is called by the EventLoop when poll() says this fd has data to read.
// We read all available bytes and feed them to the parser.
void ClientConnection::onReadable()
{
    char buf[4096]; // Temporary buffer - 4KB is a typical chunk size

    while (true)
    {
        // recv() reads available bytes from the socket into buf.
        // Returns: >0 = bytes read, 0 = browser closed connection, -1 = error or nothing left
        ssize_t bytesRead = ::recv(fd(), buf, sizeof(buf), 0);

        if (bytesRead > 0)
        {
            // Feed the received bytes to the parser.
            // The parser may need multiple calls before it has a complete request
            // (TCP can split one HTTP request across multiple recv() calls).
            HttpRequest req;
            RequestParser::Result result = _parser.feed(
                std::string(buf, static_cast<size_t>(bytesRead)), req
            );

            if (result == RequestParser::Result::BadRequest)
            {
                // The browser sent something we cannot understand - send 400 and close
                std::cout << "  Bad request on fd=" << fd() << "\n";
                queueResponse(HttpResponse::badRequest(), false);
                _state = State::Closing;
                return;
            }

            if (result == RequestParser::Result::Complete)
            {
                // We have a full, valid HTTP request - handle it
                std::cout << "  Request complete: " << req.method << " " << req.path << "\n";
                handleRequest(req);

                // After sending the response, keep the connection open if the browser wants to
                _state = req.keepAlive ? State::Reading : State::Closing;
                return;
            }

            // Result::NeedMore - request is not complete yet, keep reading
            continue;
        }

        if (bytesRead == 0)
        {
            // Browser closed the connection cleanly
            _state = State::Closing;
            return;
        }

        // bytesRead < 0
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return; // No more data right now - poll() will tell us when there is more

        // Any other error - close the connection
        _state = State::Closing;
        return;
    }
}

// onWritable() is called by the EventLoop when poll() says this fd is ready to send.
// We flush as much of our output buffer as possible.
void ClientConnection::onWritable()
{
    while (!_out.empty())
    {
        // send() tries to write bytes from our output buffer to the socket.
        // It may not send everything at once (partial write) - that is normal.
        ssize_t bytesSent = ::send(fd(), _out.data(), _out.size(), 0);

        if (bytesSent > 0)
        {
            // Remove the bytes that were successfully sent
            _out.erase(0, static_cast<size_t>(bytesSent));
            continue; // Try to send more
        }

        if (bytesSent < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
            return; // Socket buffer is full - poll() will call us again when ready

        // Any other error - close the connection
        _state = State::Closing;
        return;
    }
    // Output buffer is now empty - response has been fully sent.
    // shouldRemove() will pick this up if state is Closing.
}

// queueResponse() serializes an HttpResponse into raw bytes and stores them
// in the output buffer (_out). The EventLoop will call onWritable() to send them.
void ClientConnection::queueResponse(const HttpResponse& resp, bool keepAlive)
{
    // toRaw() converts the structured response to a proper HTTP response string:
    // "HTTP/1.1 200 OK\r\nContent-Length: 42\r\n\r\n<html>..."
    _out = resp.toRaw(keepAlive);

    // If keep-alive is false, mark this connection for closing after the response is sent
    if (!keepAlive)
        _state = State::Closing;
    else
        _state = State::Writing;
}

// handleRequest() is the "controller" - it decides what to do with a completed request.
// It asks the Router which handler to use, then calls that handler to build the response.
void ClientConnection::handleRequest(const HttpRequest& req)
{
    // Ask the Router: given this request, what should we do?
    // (later: Router will use the config file to match location blocks)
    RouteDecision decision = _router.route(req);

    // Only allow the three methods the subject requires
    if (req.method != "GET" && req.method != "POST" && req.method != "DELETE")
    {
        queueResponse(HttpResponse::methodNotAllowed(), req.keepAlive);
        return;
    }

    // Pick the right handler based on the routing decision
    HttpResponse resp;
    if (decision.isCgi)
        resp = Handlers::handleCgi(req, decision);         // Run a CGI script (Phase 6)
    else if (req.method == "POST" && decision.allowUpload)
        resp = Handlers::handleUpload(req, decision);      // Handle file upload (Phase 5)
    else
        resp = Handlers::serveStatic(req, decision);       // Serve a file from disk (Phase 3)

    queueResponse(resp, req.keepAlive);
}
