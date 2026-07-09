#include "ClientConnection.hpp"
#include <sys/socket.h>
#include <sys/wait.h>
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

ClientConnection::ClientConnection(int fd, const ServerConfig& config) : _fd(fd), _router(config)
{
    // Start in Reading state - we always wait for the browser to speak first
}

// wantedEvents() tells the EventLoop which poll() events we care about right now.
// This changes depending on our current state.
short ClientConnection::wantedEvents() const
{
    short events = 0;

    // In CGI state we do not want to read from the client socket.
    // we are waiting for the CGI pipe instead (EventLoop handles that seperately)
    if (_state != State::Closing && _state != State::CGI)
        events |= POLLIN;

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
            // before it was: RequestParser
            HttpRequestParser::Result result = _parser.feed(
                std::string(buf, static_cast<size_t>(bytesRead)), req
            );

            if (result == HttpRequestParser::Result::BadRequest) // before it was RequestParser
            {
                // The browser sent something we cannot understand - send 400 and close
                std::cout << "  Bad request on fd=" << fd() << "\n";
                queueResponse(HttpResponse::badRequest(), false);
                _state = State::Closing;
                return;
            }

            if (result == HttpRequestParser::Result::Complete)
            {
                // We have a full, valid HTTP request - handle it
                std::cout << "  Request complete: " << req.method << " " << req.path << "\n";
                handleRequest(req);

                // After sending the response, keep the connection open if the browser wants to
                // _state = req.keepAlive ? State::Reading : State::Closing;
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
void ClientConnection::handleRequest(const HttpRequest& req)
{
    RouteDecision decision = _router.route(req);

    Handler handler;
    HttpResponse resp = handler.handle(decision, req);

    queueResponse(resp, req.keepAlive);
}


//OLD!! changed for the one above on 19 May
// // handleRequest() is the "controller" - it decides what to do with a completed request.
// // It asks the Router which handler to use, then calls that handler to build the response.
//
// void ClientConnection::handleRequest(const HttpRequest& req)
// {
//     // Ask the Router: given this request, what should we do?
//     // (later: Router will use the config file to match location blocks)
//     RouteDecision decision = _router.route(req);

//     // Only allow the three methods the subject requires
//     if (req.method != "GET" && req.method != "POST" && req.method != "DELETE")
//     {
//         queueResponse(HttpResponse::methodNotAllowed(), req.keepAlive);
//         return;
//     }

//     // Pick the right handler based on the routing decision
//     HttpResponse resp;
//     if (decision.isCgi)
//         resp = Handlers::handleCgi(req, decision);         // Run a CGI script (Phase 6)
//     else if (req.method == "POST" && decision.allowUpload)
//         resp = Handlers::handleUpload(req, decision);      // Handle file upload (Phase 5)
//     else
//         resp = Handlers::serveStatic(req, decision);       // Serve a file from disk (Phase 3)

//     queueResponse(resp, req.keepAlive);
// }

// startCgi() forks a child process to run a CGI script.
// The child runs the script via execve(). The parent keeps the
// read end of the pipe so EventLoop can poll() it for output
void ClientConnection::startCgi(const std::string& executable,
                                const std::string& scriptPath,
                                const std::vector<std::string>& env,
                                const std::string& body)
{
    (void)body; // temporarily unused, Sara will wire in the request body later
    // Create a pipe: read end, pipefd[1] = write end
    int pipefd[2];
    if (::pipe(pipefd) < 0)
    {
        queueResponse(HttpResponse::text(500, "Internal Server Error"), false);
        return;
    }
    _cgiPid = ::fork();

    if (_cgiPid < 0)
    {
        // fork() failed
        ::close(pipefd[0]);
        ::close(pipefd[1]);
        queueResponse(HttpResponse::text(500, "Internal Server Error"), false);
        return;
    }
    if (_cgiPid == 0)
    {
        // CHILD Process: redirect stdout to the write end of the pipe
        ::close(pipefd[0]); // child does not need the read end
        ::dup2(pipefd[1], STDOUT_FILENO);
        ::close(pipefd[1]);

        // Build the argv array for execve()
        std::vector<char*> argv;
        argv.push_back(const_cast<char*>(executable.c_str()));
        argv.push_back(const_cast<char*>(scriptPath.c_str()));
        argv.push_back(nullptr);

        // Build  the envp array for execve()
        std::vector<char*> envp;
        for (const std::string& e : env)
            envp.push_back(const_cast<char*>(e.c_str()));
        envp.push_back(nullptr);

        ::execve(executable.c_str(), argv.data(), envp.data());

        // If execve() returns, something went wrong
        ::exit(1);
    }
    // PARENT PROCESS: keep the read end, close the write end
    ::close(pipefd[1]);
    _cgiFd = pipefd[0];
    _state = State::CGI;
}

// onCgiReadable() is called by EventLoop when the CGI pipe has data
// We accumulate the output and build a response when the pipe closes.
void ClientConnection::onCgiReadable()
{
    char buf[4096];
    ssize_t bytesRead = ::read(_cgiFd, buf, sizeof(buf));
    if (bytesRead > 0)
    {
        _cgiOutput += std::string(buf, static_cast<size_t>(bytesRead));
        return; // More data might come, wait for next poll() call
    }
    // bytesRead == 0 means the pipe closed, script is done
    ::close(_cgiFd);
    _cgiFd = -1;

    // Wait for the child process to finish (non-blocking)
    if (_cgiPid != -1)
    {
        ::waitpid(_cgiPid, nullptr, WNOHANG);
        _cgiPid = -1;
    }

    // Build the response from the CGI output
    HttpResponse resp;
    resp.status = 200;
    resp.reason = "OK";
    resp.body = _cgiOutput;
    resp.headers["Content-Type"] = "text/html";
    _cgiOutput.clear();

    queueResponse(resp, false);
}
