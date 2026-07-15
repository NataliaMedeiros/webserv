#include "ClientConnection.hpp"
#include "FileSystem.hpp"

#include <sys/socket.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>

#include <cerrno>
#include <cctype>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <utility>
#include <vector>

ClientConnection::ClientConnection(int fd, const ServerConfig& config)
    : _fd(fd)
    , _state(State::Reading)
    , _router(config)
    , _parser([this](const std::string& path) -> size_t {
        return _router.maxBodySizeFor(path);
    })
    , _cgiPid(-1)
    , _cgiInputFd(-1)
    , _cgiOutputFd(-1)
    , _cgiInputOffset(0)
    , _cgiInputClosed(true)
    , _cgiOutputClosed(true)
    , _cgiKeepAlive(false)
    , _cgiSuppressBody(false)
    , _cgiExited(false)
    , _cgiExitStatus(0)
{
}

ClientConnection::~ClientConnection()
{
    cleanupCgi();
}

short ClientConnection::wantedEvents() const
{
    short events = 0;

    if (_state != State::Closing && _state != State::CGI)
        events |= POLLIN;

    if (!_out.empty())
        events |= POLLOUT;

    return events;
}

bool ClientConnection::shouldRemove() const
{
    return _state == State::Closing && _out.empty();
}

bool ClientConnection::hasCgiInputPipe() const
{
    return _cgiInputFd >= 0;
}

bool ClientConnection::hasCgiOutputPipe() const
{
    return _cgiOutputFd >= 0;
}

int ClientConnection::cgiInputFd() const
{
    return _cgiInputFd;
}

int ClientConnection::cgiOutputFd() const
{
    return _cgiOutputFd;
}

void ClientConnection::onReadable()
{
    char buffer[4096];

    while (true)
    {
        const ssize_t bytesRead = ::recv(fd(), buffer, sizeof(buffer), 0);

        if (bytesRead > 0)
        {
            HttpRequest request;
            const HttpRequestParser::Result result = _parser.feed(
                std::string(buffer, static_cast<size_t>(bytesRead)),
                request
            );

            if (result == HttpRequestParser::Result::PayloadTooLarge)
            {
                queueResponse(
                    HttpResponse::error(413, "Payload Too Large"),
                    false
                );
                return;
            }

            if (result == HttpRequestParser::Result::UriTooLong)
            {
                queueResponse(
                    HttpResponse::error(414, "URI Too Long"),
                    false
                );
                return;
            }

            if (result == HttpRequestParser::Result::BadRequest)
            {
                std::cout << "  Bad request on fd=" << fd() << "\n";
                queueResponse(HttpResponse::badRequest(), false);
                return;
            }

            if (result == HttpRequestParser::Result::Complete)
            {
                std::cout << "  Request complete: "
                          << request.method << ' '
                          << request.path << "\n";

                handleRequest(std::move(request));
                return;
            }

            continue;
        }

        if (bytesRead == 0)
        {
            _state = State::Closing;
            return;
        }

        if (errno == EINTR)
            continue;

        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return;

        _state = State::Closing;
        return;
    }
}

void ClientConnection::onWritable()
{
    while (!_out.empty())
    {
        const ssize_t bytesSent = ::send(
            fd(),
            _out.data(),
            _out.size(),
            0
        );

        if (bytesSent > 0)
        {
            _out.erase(0, static_cast<size_t>(bytesSent));
            continue;
        }

        if (bytesSent < 0 && errno == EINTR)
            continue;

        if (bytesSent < 0 &&
            (errno == EAGAIN || errno == EWOULDBLOCK))
        {
            return;
        }

        _state = State::Closing;
        return;
    }

    if (_state == State::Writing)
        _state = State::Reading;
}

void ClientConnection::queueResponse(
    const HttpResponse& response,
    bool keepAlive,
    bool suppressBody
)
{
    if (suppressBody)
    {
        HttpResponse headResponse = response;
        headResponse.body.clear();
        _out = headResponse.toRaw(keepAlive);
    }
    else
    {
        _out = response.toRaw(keepAlive);
    }

    _state = keepAlive ? State::Writing : State::Closing;
}

void ClientConnection::handleRequest(HttpRequest&& request)
{
    const RouteDecision decision = _router.route(request);
    const std::string fullPath = buildRequestPath(decision, request);

    if (shouldUseCgi(decision, fullPath))
    {
        if (!isMethodAllowed(decision, request.method))
        {
            HttpResponse response = HttpResponse::methodNotAllowed();
            response.setHeader("Allow", joinAllowedMethods(decision));

            queueResponse(
                response,
                request.keepAlive,
                request.method == "HEAD"
            );
            return;
        }

        // With the current configuration format, an empty cgiExtension is
        // treated as interpreter mode: /usr/bin/python3 script.py.
        // In that mode, the requested script must exist.
        if (decision.cgiExtension.empty() &&
            (!FileSystem::exists(fullPath) ||
             !FileSystem::isFileNormal(fullPath)))
        {
            Handler handler;
            const HttpResponse response = handler.handle(decision, request);

            queueResponse(
                response,
                request.keepAlive,
                request.method == "HEAD"
            );
            return;
        }

        startCgi(decision, std::move(request), fullPath);
        return;
    }

    Handler handler;
    const HttpResponse response = handler.handle(decision, request);

    queueResponse(
        response,
        request.keepAlive,
        request.method == "HEAD"
    );
}

bool ClientConnection::setNonBlocking(int descriptor)
{
    const int flags = ::fcntl(descriptor, F_GETFL, 0);

    if (flags == -1)
        return false;

    return ::fcntl(
        descriptor,
        F_SETFL,
        flags | O_NONBLOCK
    ) != -1;
}

std::string ClientConnection::buildRequestPath(
    const RouteDecision& decision,
    const HttpRequest& request
) const
{
    std::string root = decision.root;

    if (!root.empty() && root[root.size() - 1] == '/')
        root.erase(root.size() - 1);

    if (request.path == "/")
        return root + "/" + decision.index;

    std::string remainder = request.path;

    if (!decision.locationPath.empty() &&
        decision.locationPath != "/" &&
        remainder.compare(
            0,
            decision.locationPath.size(),
            decision.locationPath
        ) == 0)
    {
        remainder = remainder.substr(decision.locationPath.size());
    }

    if (remainder.empty() || remainder[0] != '/')
        remainder = "/" + remainder;

    return root + remainder;
}

bool ClientConnection::shouldUseCgi(
    const RouteDecision& decision,
    const std::string& fullPath
) const
{
    if (decision.cgiPass.empty())
        return false;

    if (decision.cgiExtension.empty())
        return true;

    if (fullPath.size() < decision.cgiExtension.size())
        return false;

    return fullPath.compare(
        fullPath.size() - decision.cgiExtension.size(),
        decision.cgiExtension.size(),
        decision.cgiExtension
    ) == 0;
}

bool ClientConnection::isMethodAllowed(
    const RouteDecision& decision,
    const std::string& method
) const
{
    if (decision.methods.empty())
        return true;

    for (size_t i = 0; i < decision.methods.size(); ++i)
    {
        if (decision.methods[i] == method)
            return true;
    }

    return false;
}

std::string ClientConnection::joinAllowedMethods(
    const RouteDecision& decision
) const
{
    std::string result;

    for (size_t i = 0; i < decision.methods.size(); ++i)
    {
        if (i != 0)
            result += ", ";

        result += decision.methods[i];
    }

    return result;
}

std::string ClientConnection::toUpperCgiName(
    const std::string& headerName
) const
{
    std::string result;

    for (size_t i = 0; i < headerName.size(); ++i)
    {
        unsigned char character =
            static_cast<unsigned char>(headerName[i]);

        if (character == '-')
            result += '_';
        else
            result += static_cast<char>(std::toupper(character));
    }

    return result;
}

std::string ClientConnection::lowerCopy(
    const std::string& value
) const
{
    std::string result = value;

    for (size_t i = 0; i < result.size(); ++i)
    {
        result[i] = static_cast<char>(std::tolower(
            static_cast<unsigned char>(result[i])
        ));
    }

    return result;
}

std::string ClientConnection::trimCopy(
    const std::string& value
) const
{
    size_t first = 0;

    while (first < value.size() &&
           std::isspace(static_cast<unsigned char>(value[first])))
    {
        ++first;
    }

    size_t last = value.size();

    while (last > first &&
           std::isspace(static_cast<unsigned char>(value[last - 1])))
    {
        --last;
    }

    return value.substr(first, last - first);
}

std::vector<std::string> ClientConnection::buildCgiEnvironment(
    const HttpRequest& request,
    const std::string& fullPath
) const
{
    std::vector<std::string> environment;

    environment.push_back("GATEWAY_INTERFACE=CGI/1.1");
    environment.push_back("SERVER_SOFTWARE=webserv");
    environment.push_back(
        "SERVER_PROTOCOL=" +
        (request.version.empty() ? std::string("HTTP/1.1") : request.version)
    );
    environment.push_back("REQUEST_METHOD=" + request.method);
    environment.push_back("SCRIPT_FILENAME=" + fullPath);
    environment.push_back("SCRIPT_NAME=" + request.path);
    environment.push_back("PATH_INFO=" + request.path);
    environment.push_back("QUERY_STRING=" + request.query_string);
    environment.push_back(
        "CONTENT_LENGTH=" + std::to_string(request.body.size())
    );

    for (std::map<std::string, std::string>::const_iterator it =
             request.headers.begin();
         it != request.headers.end();
         ++it)
    {
        const std::string lowerName = lowerCopy(it->first);

        if (lowerName == "content-length")
            continue;

        if (lowerName == "content-type")
        {
            environment.push_back("CONTENT_TYPE=" + it->second);
            continue;
        }

        environment.push_back(
            "HTTP_" + toUpperCgiName(it->first) + "=" + it->second
        );
    }

    return environment;
}

void ClientConnection::startCgi(
    const RouteDecision& decision,
    HttpRequest&& request,
    const std::string& fullPath
)
{
    if (decision.cgiPass.empty())
    {
        queueResponse(
            HttpResponse::error(500, "Internal Server Error"),
            false
        );
        return;
    }

    int inputPipe[2];
    int outputPipe[2];

    if (::pipe(inputPipe) == -1)
    {
        queueResponse(
            HttpResponse::error(500, "Internal Server Error"),
            false
        );
        return;
    }

    if (::pipe(outputPipe) == -1)
    {
        ::close(inputPipe[0]);
        ::close(inputPipe[1]);

        queueResponse(
            HttpResponse::error(500, "Internal Server Error"),
            false
        );
        return;
    }

    const std::vector<std::string> environment =
        buildCgiEnvironment(request, fullPath);

    const bool interpreterMode = decision.cgiExtension.empty();

    _cgiPid = ::fork();

    if (_cgiPid == -1)
    {
        ::close(inputPipe[0]);
        ::close(inputPipe[1]);
        ::close(outputPipe[0]);
        ::close(outputPipe[1]);

        queueResponse(
            HttpResponse::error(500, "Internal Server Error"),
            false
        );
        return;
    }

    if (_cgiPid == 0)
    {
        if (::dup2(inputPipe[0], STDIN_FILENO) == -1)
            ::_exit(1);

        if (::dup2(outputPipe[1], STDOUT_FILENO) == -1)
            ::_exit(1);

        ::close(inputPipe[0]);
        ::close(inputPipe[1]);
        ::close(outputPipe[0]);
        ::close(outputPipe[1]);

        std::vector<char*> environmentPointers;

        for (size_t i = 0; i < environment.size(); ++i)
        {
            environmentPointers.push_back(
                const_cast<char*>(environment[i].c_str())
            );
        }

        environmentPointers.push_back(NULL);

        std::vector<char*> arguments;
        arguments.push_back(
            const_cast<char*>(decision.cgiPass.c_str())
        );

        if (interpreterMode)
        {
            arguments.push_back(
                const_cast<char*>(fullPath.c_str())
            );
        }

        arguments.push_back(NULL);

        ::execve(
            decision.cgiPass.c_str(),
            arguments.data(),
            environmentPointers.data()
        );

        ::_exit(1);
    }

    ::close(inputPipe[0]);
    ::close(outputPipe[1]);

    _cgiInputFd = inputPipe[1];
    _cgiOutputFd = outputPipe[0];

    if (!setNonBlocking(_cgiInputFd) ||
        !setNonBlocking(_cgiOutputFd))
    {
        ::close(_cgiInputFd);
        ::close(_cgiOutputFd);

        _cgiInputFd = -1;
        _cgiOutputFd = -1;

        ::kill(_cgiPid, SIGKILL);
        ::waitpid(_cgiPid, NULL, 0);
        _cgiPid = -1;

        queueResponse(
            HttpResponse::error(500, "Internal Server Error"),
            false
        );
        return;
    }

    _cgiInput = std::move(request.body);
    _cgiInputOffset = 0;
    _cgiOutput.clear();

    _cgiKeepAlive = request.keepAlive;
    _cgiSuppressBody = request.method == "HEAD";

    _cgiInputClosed = false;
    _cgiOutputClosed = false;
    _cgiExited = false;
    _cgiExitStatus = 0;

    if (_cgiInput.empty())
    {
        ::close(_cgiInputFd);
        _cgiInputFd = -1;
        _cgiInputClosed = true;
    }

    _state = State::CGI;
}

void ClientConnection::onCgiInputWritable()
{
    if (_cgiInputFd < 0)
        return;

    while (_cgiInputOffset < _cgiInput.size())
    {
        const ssize_t written = ::write(
            _cgiInputFd,
            _cgiInput.data() + _cgiInputOffset,
            _cgiInput.size() - _cgiInputOffset
        );

        if (written > 0)
        {
            _cgiInputOffset += static_cast<size_t>(written);
            continue;
        }

        if (written < 0 && errno == EINTR)
            continue;

        if (written < 0 &&
            (errno == EAGAIN || errno == EWOULDBLOCK))
        {
            return;
        }

        failCgi(502, "Bad Gateway");
        return;
    }

    ::close(_cgiInputFd);
    _cgiInputFd = -1;
    _cgiInputClosed = true;
    _cgiInputOffset = 0;

    std::string().swap(_cgiInput);

    checkCgiFinished();
}

void ClientConnection::onCgiOutputReadable()
{
    if (_cgiOutputFd < 0)
        return;

    char buffer[8192];

    while (true)
    {
        const ssize_t count = ::read(
            _cgiOutputFd,
            buffer,
            sizeof(buffer)
        );

        if (count > 0)
        {
            _cgiOutput.append(
                buffer,
                static_cast<size_t>(count)
            );
            continue;
        }

        if (count == 0)
        {
            ::close(_cgiOutputFd);
            _cgiOutputFd = -1;
            _cgiOutputClosed = true;

            checkCgiFinished();
            return;
        }

        if (errno == EINTR)
            continue;

        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return;

        failCgi(502, "Bad Gateway");
        return;
    }
}

void ClientConnection::checkCgiFinished()
{
    if (_cgiPid <= 0)
    {
        if (_cgiExited && _cgiOutputClosed)
            finishCgi();

        return;
    }

    int status = 0;
    const pid_t result = ::waitpid(_cgiPid, &status, WNOHANG);

    if (result == 0)
        return;

    if (result == -1)
    {
        failCgi(502, "Bad Gateway");
        return;
    }

    _cgiPid = -1;
    _cgiExited = true;
    _cgiExitStatus = status;

    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
    {
        failCgi(502, "Bad Gateway");
        return;
    }

    if (_cgiOutputClosed)
        finishCgi();
}

HttpResponse ClientConnection::parseCgiOutput(
    const std::string& output
) const
{
    HttpResponse response;
    response.status = 200;
    response.reason = HttpResponse::reasonPhrase(200);

    size_t separator = output.find("\r\n\r\n");
    size_t separatorSize = 4;

    if (separator == std::string::npos)
    {
        separator = output.find("\n\n");
        separatorSize = 2;
    }

    if (separator == std::string::npos)
    {
        response.setBody(output, "text/html; charset=utf-8");
        return response;
    }

    const std::string headerBlock = output.substr(0, separator);
    const std::string body = output.substr(separator + separatorSize);

    std::istringstream headerStream(headerBlock);
    std::string line;

    while (std::getline(headerStream, line))
    {
        if (!line.empty() && line[line.size() - 1] == '\r')
            line.erase(line.size() - 1);

        if (line.empty())
            continue;

        const size_t colon = line.find(':');

        if (colon == std::string::npos)
            continue;

        const std::string name = trimCopy(line.substr(0, colon));
        const std::string value = trimCopy(line.substr(colon + 1));

        if (lowerCopy(name) == "status")
        {
            const size_t space = value.find(' ');
            const std::string statusText = value.substr(0, space);
            const int statusCode = std::atoi(statusText.c_str());

            if (statusCode >= 100 && statusCode <= 599)
            {
                response.status = statusCode;

                if (space != std::string::npos)
                    response.reason = trimCopy(value.substr(space + 1));
                else
                    response.reason = HttpResponse::reasonPhrase(statusCode);
            }

            continue;
        }

        response.setHeader(name, value);
    }

    response.body = body;

    if (!response.hasHeader("Content-Type") && !body.empty())
    {
        response.setHeader(
            "Content-Type",
            "text/html; charset=utf-8"
        );
    }

    return response;
}

void ClientConnection::finishCgi()
{
    const bool keepAlive = _cgiKeepAlive;
    const bool suppressBody = _cgiSuppressBody;
    const HttpResponse response = parseCgiOutput(_cgiOutput);

    cleanupCgi();
    queueResponse(response, keepAlive, suppressBody);
}

void ClientConnection::failCgi(
    int status,
    const std::string& reason
)
{
    cleanupCgi();
    queueResponse(HttpResponse::error(status, reason), false);
}

void ClientConnection::cleanupCgi()
{
    if (_cgiInputFd >= 0)
    {
        ::close(_cgiInputFd);
        _cgiInputFd = -1;
    }

    if (_cgiOutputFd >= 0)
    {
        ::close(_cgiOutputFd);
        _cgiOutputFd = -1;
    }

    if (_cgiPid > 0)
    {
        ::kill(_cgiPid, SIGKILL);
        ::waitpid(_cgiPid, NULL, 0);
        _cgiPid = -1;
    }

    _cgiInputOffset = 0;
    _cgiInputClosed = true;
    _cgiOutputClosed = true;
    _cgiKeepAlive = false;
    _cgiSuppressBody = false;
    _cgiExited = false;
    _cgiExitStatus = 0;

    std::string().swap(_cgiInput);
    std::string().swap(_cgiOutput);
}




// #include "ClientConnection.hpp"
// #include <sys/socket.h>
// #include <sys/wait.h>
// #include <cerrno>
// #include <iostream>
// #include <fcntl.h>
// #include <unistd.h>
// #include <utility>

// // ClientConnection represents one open TCP connection with one browser.
// // It is a state machine - always in exactly one of three states:
// //
// //   Reading  -> waiting for and receiving bytes from the browser
// //   Writing  -> a response is ready and we are sending it back
// //   Closing  -> we are done, EventLoop will remove this connection
// //
// // State transitions:
// //   Reading -> Writing  : request fully received and parsed, response queued
// //   Writing -> Reading  : response fully sent, keep-alive is active
// //   Writing -> Closing  : response fully sent, connection: close
// //   Reading -> Closing  : browser disconnected or sent a bad request

// ClientConnection::ClientConnection(int fd, const ServerConfig& config)
//     : _fd(fd), _router(config),
//     _parser([this](const std::string& path) -> size_t{return _router.maxBodySizeFor(path);})
// {
// }

// // wantedEvents() tells the EventLoop which poll() events we care about right now.
// // This changes depending on our current state.
// short ClientConnection::wantedEvents() const
// {
//     short events = 0;

//     // In CGI state we do not want to read from the client socket.
//     // we are waiting for the CGI pipe instead (EventLoop handles that seperately)
//     if (_state != State::Closing && _state != State::CGI)
//         events |= POLLIN;

//     if (!_out.empty())
//         events |= POLLOUT;

//     return events;
// }

// // shouldRemove() returns true when the EventLoop can safely delete this connection.
// // We wait until both conditions are true: state is Closing AND output buffer is empty.
// // This ensures we never drop the connection before finishing sending the response.
// bool ClientConnection::shouldRemove() const
// {
//     return (_state == State::Closing) && _out.empty();
// }

// // onReadable() is called by the EventLoop when poll() says this fd has data to read.
// // We read all available bytes and feed them to the parser.
// void ClientConnection::onReadable()
// {
//     char buf[4096]; // Temporary buffer - 4KB is a typical chunk size

//     while (true)
//     {
//         // recv() reads available bytes from the socket into buf.
//         // Returns: >0 = bytes read, 0 = browser closed connection, -1 = error or nothing left
//         ssize_t bytesRead = ::recv(fd(), buf, sizeof(buf), 0);

//         if (bytesRead > 0)
//         {
//             // Feed the received bytes to the parser.
//             // The parser may need multiple calls before it has a complete request
//             // (TCP can split one HTTP request across multiple recv() calls).
//             HttpRequest req;
//             // before it was: RequestParser
//             HttpRequestParser::Result result = _parser.feed(
//                 std::string(buf, static_cast<size_t>(bytesRead)), req
//             );

//             if (result == HttpRequestParser::Result::PayloadTooLarge)
//             {
//                 std::cout << "Sending status: " << HttpResponse::error(413, "Payload Too Large").status << "\n";
//                 queueResponse(HttpResponse::error(413, "Payload Too Large"), false);
//                 _state = State::Closing;
//                 return;
//             }
//             if (result == HttpRequestParser::Result::UriTooLong)
//             {
//                 std::cout << "Sending status: " << HttpResponse::error(414, "URI Too Long").status << "\n";
//                 queueResponse(HttpResponse::error(414, "URI Too Long"), false);
//                 _state = State::Closing;
//                 return;
//             }
//             if (result == HttpRequestParser::Result::BadRequest) // before it was RequestParser
//             {
//                 // The browser sent something we cannot understand - send 400 and close
//                 std::cout << "  Bad request on fd=" << fd() << "\n";
//                 queueResponse(HttpResponse::badRequest(), false);
//                 _state = State::Closing;
//                 return;
//             }

//             if (result == HttpRequestParser::Result::Complete)
//             {
//                 // We have a full, valid HTTP request - handle it
//                 std::cout << "  Request complete: " << req.method << " " << req.path << "\n";
//                 // handleRequest(req);
//                 handleRequest(std::move(req));

//                 // After sending the response, keep the connection open if the browser wants to
//                 // _state = req.keepAlive ? State::Reading : State::Closing;
//                 return;
//             }

//             // Result::NeedMore - request is not complete yet, keep reading
//             continue;
//         }

//         if (bytesRead == 0)
//         {
//             // Browser closed the connection cleanly
//             _state = State::Closing;
//             return;
//         }

//         // bytesRead < 0
//         if (errno == EAGAIN || errno == EWOULDBLOCK)
//             return; // No more data right now - poll() will tell us when there is more

//         // Any other error - close the connection
//         _state = State::Closing;
//         return;
//     }
// }

// // onWritable() is called by the EventLoop when poll() says this fd is ready to send.
// // We flush as much of our output buffer as possible.
// void ClientConnection::onWritable()
// {
//     while (!_out.empty())
//     {
//         // send() tries to write bytes from our output buffer to the socket.
//         // It may not send everything at once (partial write) - that is normal.
//         ssize_t bytesSent = ::send(fd(), _out.data(), _out.size(), 0);

//         if (bytesSent > 0)
//         {
//             // Remove the bytes that were successfully sent
//             _out.erase(0, static_cast<size_t>(bytesSent));
//             continue; // Try to send more
//         }

//         if (bytesSent < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
//             return; // Socket buffer is full - poll() will call us again when ready

//         // Any other error - close the connection
//         _state = State::Closing;
//         return;
//     }
//     // Output buffer is now empty - response has been fully sent.
//     // shouldRemove() will pick this up if state is Closing.
// }

// // queueResponse() serializes an HttpResponse into raw bytes and stores them
// // in the output buffer (_out). The EventLoop will call onWritable() to send them.
// // void ClientConnection::queueResponse(const HttpResponse& resp, bool keepAlive)
// // {
//     // toRaw() converts the structured response to a proper HTTP response string:
//     // "HTTP/1.1 200 OK\r\nContent-Length: 42\r\n\r\n<html>..."
// //     _out = resp.toRaw(keepAlive);

// //     // If keep-alive is false, mark this connection for closing after the response is sent
// //     if (!keepAlive)
// //         _state = State::Closing;
// //     else
// //         _state = State::Writing;
// // }

// void ClientConnection::queueResponse(const HttpResponse& resp, bool keepAlive, bool suppressBody)
// {
//     _out = resp.toRaw(keepAlive, suppressBody);

//     if (keepAlive)
//         _state = State::Writing;
//     else
//         _state = State::Closing;
// }
// // void ClientConnection::handleRequest(HttpRequest& req)
// // {
// //     RouteDecision decision = _router.route(req);

// //     Handler handler;
// //     HttpResponse resp = handler.handle(decision, req);

// //     queueResponse(resp, req.keepAlive);
// // }

// void ClientConnection::handleRequest(HttpRequest&& req)
// {
//     RouteDecision decision = _router.route(req);

//     Handler handler;

//     if (handler.shouldUseCgi(decision, req))
//     {
//         const std::string fullPath =
//             handler.buildRequestPath(decision, req);

//         startCgi(
//             decision,
//             std::move(req),
//             fullPath
//         );

//         return;
//     }

//     HttpResponse resp =
//         handler.handle(decision, req);

//     queueResponse(
//         resp,
//         req.keepAlive,
//         req.method == "HEAD"
//     );
// }

// //OLD!! changed for the one above on 19 May
// // // handleRequest() is the "controller" - it decides what to do with a completed request.
// // // It asks the Router which handler to use, then calls that handler to build the response.
// //
// // void ClientConnection::handleRequest(const HttpRequest& req)
// // {
// //     // Ask the Router: given this request, what should we do?
// //     // (later: Router will use the config file to match location blocks)
// //     RouteDecision decision = _router.route(req);

// //     // Only allow the three methods the subject requires
// //     if (req.method != "GET" && req.method != "POST" && req.method != "DELETE")
// //     {
// //         queueResponse(HttpResponse::methodNotAllowed(), req.keepAlive);
// //         return;
// //     }

// //     // Pick the right handler based on the routing decision
// //     HttpResponse resp;
// //     if (decision.isCgi)
// //         resp = Handlers::handleCgi(req, decision);         // Run a CGI script (Phase 6)
// //     else if (req.method == "POST" && decision.allowUpload)
// //         resp = Handlers::handleUpload(req, decision);      // Handle file upload (Phase 5)
// //     else
// //         resp = Handlers::serveStatic(req, decision);       // Serve a file from disk (Phase 3)

// //     queueResponse(resp, req.keepAlive);
// // }

// // startCgi() forks a child process to run a CGI script.
// // The child runs the script via execve(). The parent keeps the
// // read end of the pipe so EventLoop can poll() it for output
// // void ClientConnection::startCgi(const std::string& executable,
// //                                 const std::string& scriptPath,
// //                                 const std::vector<std::string>& env,
// //                                 const std::string& body)
// // {
// //     (void)body; // temporarily unused, Sara will wire in the request body later
// //     // Create a pipe: read end, pipefd[1] = write end
// //     int pipefd[2];
// //     if (::pipe(pipefd) < 0)
// //     {
// //         queueResponse(HttpResponse::text(500, "Internal Server Error"), false);
// //         return;
// //     }
// //     _cgiPid = ::fork();

// //     if (_cgiPid < 0)
// //     {
// //         // fork() failed
// //         ::close(pipefd[0]);
// //         ::close(pipefd[1]);
// //         queueResponse(HttpResponse::text(500, "Internal Server Error"), false);
// //         return;
// //     }
// //     if (_cgiPid == 0)
// //     {
// //         // CHILD Process: redirect stdout to the write end of the pipe
// //         ::close(pipefd[0]); // child does not need the read end
// //         ::dup2(pipefd[1], STDOUT_FILENO);
// //         ::close(pipefd[1]);

// //         // Build the argv array for execve()
// //         std::vector<char*> argv;
// //         argv.push_back(const_cast<char*>(executable.c_str()));
// //         argv.push_back(const_cast<char*>(scriptPath.c_str()));
// //         argv.push_back(nullptr);

// //         // Build  the envp array for execve()
// //         std::vector<char*> envp;
// //         for (const std::string& e : env)
// //             envp.push_back(const_cast<char*>(e.c_str()));
// //         envp.push_back(nullptr);

// //         ::execve(executable.c_str(), argv.data(), envp.data());

// //         // If execve() returns, something went wrong
// //         ::exit(1);
// //     }
// //     // PARENT PROCESS: keep the read end, close the write end
// //     ::close(pipefd[1]);
// //     _cgiFd = pipefd[0];
// //     _state = State::CGI;
// // }

// // onCgiReadable() is called by EventLoop when the CGI pipe has data
// // We accumulate the output and build a response when the pipe closes.
// // void ClientConnection::onCgiReadable()
// // {
// //     char buf[4096];
// //     ssize_t bytesRead = ::read(_cgiFd, buf, sizeof(buf));
// //     if (bytesRead > 0)
// //     {
// //         _cgiOutput += std::string(buf, static_cast<size_t>(bytesRead));
// //         return; // More data might come, wait for next poll() call
// //     }
// //     // bytesRead == 0 means the pipe closed, script is done
// //     ::close(_cgiFd);
// //     _cgiFd = -1;

// //     // Wait for the child process to finish (non-blocking)
// //     if (_cgiPid != -1)
// //     {
// //         ::waitpid(_cgiPid, nullptr, WNOHANG);
// //         _cgiPid = -1;
// //     }

// //     // Build the response from the CGI output
// //     HttpResponse resp;
// //     resp.status = 200;
// //     resp.reason = "OK";
// //     resp.body = _cgiOutput;
// //     resp.headers["Content-Type"] = "text/html";
// //     _cgiOutput.clear();

// //     queueResponse(resp, false);
// // }

// void ClientConnection::startCgi(
//     const RouteDecision& decision,
//     HttpRequest&& req,
//     const std::string& fullPath
// )
// {
//     int inputPipe[2];
//     int outputPipe[2];

//     if (::pipe(inputPipe) == -1)
//     {
//         queueResponse(
//             HttpResponse::error(500, "Internal Server Error"),
//             false
//         );
//         return;
//     }

//     if (::pipe(outputPipe) == -1)
//     {
//         ::close(inputPipe[0]);
//         ::close(inputPipe[1]);

//         queueResponse(
//             HttpResponse::error(500, "Internal Server Error"),
//             false
//         );
//         return;
//     }

//     _cgiPid = ::fork();

//     if (_cgiPid == -1)
//     {
//         ::close(inputPipe[0]);
//         ::close(inputPipe[1]);
//         ::close(outputPipe[0]);
//         ::close(outputPipe[1]);

//         queueResponse(
//             HttpResponse::error(500, "Internal Server Error"),
//             false
//         );
//         return;
//     }

//     if (_cgiPid == 0)
//     {
//         ::dup2(inputPipe[0], STDIN_FILENO);
//         ::dup2(outputPipe[1], STDOUT_FILENO);

//         ::close(inputPipe[0]);
//         ::close(inputPipe[1]);
//         ::close(outputPipe[0]);
//         ::close(outputPipe[1]);

//         // Construir argv e envp.
//         // Depois chamar execve().

//         ::_exit(1);
//     }

//     // Processo pai
//     ::close(inputPipe[0]);
//     ::close(outputPipe[1]);

//     _cgiInputFd = inputPipe[1];
//     _cgiOutputFd = outputPipe[0];

//     setNonBlocking(_cgiInputFd);
//     setNonBlocking(_cgiOutputFd);

//     _cgiInput = std::move(req.body);
//     _cgiInputOffset = 0;
//     _cgiOutput.clear();

//     _cgiKeepAlive = req.keepAlive;
//     _cgiSuppressBody = req.method == "HEAD";

//     _cgiInputClosed = false;
//     _cgiOutputClosed = false;

//     _state = State::CGI;

//     (void)decision;
//     (void)fullPath;
// }

// void ClientConnection::onCgiInputWritable()
// {
//     while (_cgiInputOffset < _cgiInput.size())
//     {
//         const ssize_t written = ::write(
//             _cgiInputFd,
//             _cgiInput.data() + _cgiInputOffset,
//             _cgiInput.size() - _cgiInputOffset
//         );

//         if (written > 0)
//         {
//             _cgiInputOffset +=
//                 static_cast<size_t>(written);
//             continue;
//         }

//         if (written < 0 &&
//             (errno == EAGAIN || errno == EWOULDBLOCK))
//         {
//             return;
//         }

//         cleanupCgi();
//         queueResponse(
//             HttpResponse::error(502, "Bad Gateway"),
//             false
//         );
//         return;
//     }

//     close(_cgiInputFd);
//     _cgiInputFd = -1;
//     _cgiInputClosed = true;

//     _cgiInput.clear();
//     _cgiInput.shrink_to_fit();
// }

// void ClientConnection::onCgiOutputReadable()
// {
//     char buffer[8192];

//     while (true)
//     {
//         const ssize_t count = ::read(
//             _cgiOutputFd,
//             buffer,
//             sizeof(buffer)
//         );

//         if (count > 0)
//         {
//             _cgiOutput.append(
//                 buffer,
//                 static_cast<size_t>(count)
//             );
//             continue;
//         }

//         if (count == 0)
//         {
//             close(_cgiOutputFd);
//             _cgiOutputFd = -1;
//             _cgiOutputClosed = true;

//             checkCgiFinished();
//             return;
//         }

//         if (errno == EAGAIN || errno == EWOULDBLOCK)
//             return;

//         cleanupCgi();
//         queueResponse(
//             HttpResponse::error(502, "Bad Gateway"),
//             false
//         );
//         return;
//     }
// }

// bool ClientConnection::setNonBlocking(int fd)
// {
//     const int flags = ::fcntl(fd, F_GETFL, 0);

//     if (flags == -1)
//         return false;

//     if (::fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
//         return false;

//     return true;
// }



