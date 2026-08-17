#include "ClientConnection.hpp"
#include <climits>
#include "Net.hpp"
#include <sys/socket.h>
#include <sys/wait.h>
#include <sstream>
#include <signal.h>
#include <iostream>

// A ClientConnection = an open TCP connection with a browser.
// It can be in one of the 3 states:
//   Reading  -> waiting for and receiving bytes from browser
//   Writing  -> response is ready and we are sending it back
//   Closing  -> done! EventLoop will remove this connection
//
// State transitions:
//   Reading -> Writing  : request fully received and parsed, response queued
//   Writing -> Reading  : response fully sent, keep-alive is active
//   Writing -> Closing  : response fully sent, connection: close
//   Reading -> Closing  : browser disconnected or sent a bad request
ClientConnection::ClientConnection(int fd, const ServerConfig& config)
    : _fd(fd), _router(config),
    _parser([this](const std::string& path) -> size_t{return _router.maxBodySizeFor(path);}),
    _lastActivity(::time(nullptr))
{
}

// Safety net for CGI resources. Most of the time these are already closed
// by the specific code path that ends the connection (killCgi() on a
// timeout, or the EOF handling in onCgiReadable() once the script is
// done). But if the connection is removed some other way while a CGI
// is still in flight, e.g. EventLoop sees the client socket hang up
// (POLLERR/POLLHUP/POLLNVAL) mid-script, nothing else would ever close
// these. Without this, the pipe fds leak and the child process is left
// unreaped (becoming a zombie).
ClientConnection::~ClientConnection()
{
    if (_cgiPid != -1)
    {
        ::kill(_cgiPid, SIGKILL);
        ::waitpid(_cgiPid, nullptr, 0);
    }
    if (_cgiFd != -1)
        ::close(_cgiFd);
    if (_cgiStdinFd != -1)
        ::close(_cgiStdinFd);
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
// void ClientConnection::onReadable()
void ClientConnection::onReadable()
{
    char buf[4096]; // Temporary buffer - 4KB is a typical chunk size

    // Exactly one recv() attempt per call. If there's more data, poll()
    // will flag this fd as readable again and EventLoop will call us again,
    // we never read in a loop without going back through poll() first.
    ssize_t bytesRead = ::recv(fd(), buf, sizeof(buf), 0);

    if (bytesRead > 0)
    {
        _lastActivity = ::time(nullptr);
        // Feed the received bytes to the parser.
        // The parser may need multiple calls before it has a complete request
        // (TCP can split one HTTP request across multiple recv() calls,
        // each one arriving through its own poll() cycle).
        HttpRequest req;
        HttpRequestParser::Result result = _parser.feed(
            std::string(buf, static_cast<size_t>(bytesRead)), req
        );
        if (result == HttpRequestParser::Result::PayloadTooLarge)
        {
            queueResponse(HttpResponse::error(413, "Payload Too Large"), false);
            _state = State::Closing;
            return;
        }
        if (result == HttpRequestParser::Result::UriTooLong)
        {
            queueResponse(HttpResponse::error(414, "URI Too Long"), false);
            _state = State::Closing;
            return;
        }
        if (result == HttpRequestParser::Result::BadRequest)
        {
            std::cout << "  Bad request on fd=" << fd() << "\n";
            queueResponse(HttpResponse::badRequest(), false);
            _state = State::Closing;
            return;
        }
        if (result == HttpRequestParser::Result::Complete)
        {
            std::cout << "  Request complete: " << req.method << " " << req.path << "\n";
            handleRequest(req);
            return;
        }
        // Result::NeedMore - request not complete yet, just return.
        // We'll be called again once poll() sees more data.
        return;
    }

    if (bytesRead == 0)
    {
        // Browser closed the connection cleanly
        _state = State::Closing;
        return;
    }

    // bytesRead < 0: nothing to read. 
    // We don't inspect errno
    // (poll() already told us this fd was ready; genuine fd errors/hangups
    // are caught separately via POLLERR/POLLHUP/POLLNVAL in EventLoop).
    return;
}

// onWritable() is called by the EventLoop when poll() says this fd is ready to send.
// We flush as much of our output buffer as possible.
// void ClientConnection::onWritable()
void ClientConnection::onWritable()
{
    if (_out.empty())
        return;

    // Exactly one send() attempt per call. If there's more to send,
    // poll() will flag this fd as writable again next time, we never
    // send in a loop without going back through poll() first.
    ssize_t bytesSent = ::send(fd(), _out.data(), _out.size(), 0);

   if (bytesSent > 0)
    {
        _lastActivity = ::time(nullptr);
        // Remove the bytes that were successfully sent. If there's more
        // left in _out, poll() will call us again once the socket is
        // writable again.
        _out.erase(0, static_cast<size_t>(bytesSent));
        return;
    }

    // bytesSent < 0: can't send right now. We don't inspect errno; poll()
    // already told us this fd was ready, and real fd errors/hangups are
    // caught separately via POLLERR/POLLHUP/POLLNVAL in EventLoop.
    return;
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
    std::string fullPath = Handler::buildPath(decision, req);

    // If this route needs CGI, run it non-blocking via startCgi(),
    // so the whole server does not stall while a CGI script runs
    // (subject requirement).
    bool isCgiRequest = !decision.cgiPass.empty()
        && (decision.cgiExtension.empty()
            || Handler::hasExtension(fullPath, decision.cgiExtension));

    if (isCgiRequest)
    {
        std::vector<std::string> env = Handler::buildCgiEnv(decision, req, fullPath);

        // cgiPass is the interpreter (e.g. /usr/bin/python3) or the
        // script itself if it is directly executable (e.g. ./cgi_tester)
        startCgi(decision.cgiPass, fullPath, env, req.body);
        return;
    }

    Handler handler;
    HttpResponse resp = handler.handle(decision, req);

    queueResponse(resp, req.keepAlive);
}

// startCgi() forks a child process to run a CGI script.
// The child runs the script via execve(). The parent keeps the
// read end of the pipe so EventLoop can poll() it for output
void ClientConnection::startCgi(const std::string& executable,
                                const std::string& scriptPath,
                                const std::vector<std::string>& env,
                                const std::string& body)
{
    // Two pipes: outPipe carries the script's output back to us,
    // inPipe carries the request body to the script's stdin.
    int outPipe[2];
    int inPipe[2];

    if (::pipe(outPipe) < 0)
    {
        queueResponse(HttpResponse::text(500, "Internal Server Error"), false);
        return;
    }

    if (::pipe(inPipe) < 0)
    {
        ::close(outPipe[0]);
        ::close(outPipe[1]);
        queueResponse(HttpResponse::text(500, "Internal Server Error"), false);
        return;
    }

    _cgiPid = ::fork();

    if (_cgiPid < 0)
    {
        // fork() failed
        ::close(outPipe[0]);
        ::close(outPipe[1]);
        ::close(inPipe[0]);
        ::close(inPipe[1]);
        queueResponse(HttpResponse::text(500, "Internal Server Error"), false);
        return;
    }

    if (_cgiPid == 0)
    {
        // CHILD Process: stdin comes from inPipe, stdout goes to outPipe
        ::close(outPipe[0]); // child does not read its own output
        ::close(inPipe[1]);  // child does not write to its own input

        ::dup2(inPipe[0], STDIN_FILENO);
        ::dup2(outPipe[1], STDOUT_FILENO);

        ::close(inPipe[0]);
        ::close(outPipe[1]);

        // Resolve the executable to an absolute path BEFORE
        // changing directory. If "executable" is relative (e.g. "./cgi_tester")
        // it is relative to the server's original working directory, chdir'ing
        // first would break it (execve would look for it inside the script's
        // directory instead). realpath() must run before chdir().
        char resolvedExecutable[PATH_MAX];
        std::string executableToRun = executable;
        if (::realpath(executable.c_str(), resolvedExecutable) != nullptr)
            executableToRun = resolvedExecutable;
        // chdir into the script's own directory so relative file access
        // from within the CGI script works correctly.
        // Subject requirement: "The CGI should be run in the correct
        // directory for relative path file access."
        std::string scriptDir = ".";
        std::string scriptFile = scriptPath;
        size_t lastSlash = scriptPath.find_last_of('/');
        if (lastSlash != std::string::npos)
        {
            scriptDir = scriptPath.substr(0, lastSlash);
            scriptFile = scriptPath.substr(lastSlash + 1);
        }
        if (::chdir(scriptDir.c_str()) != 0)
            ::exit(1);
        // Build the argv array for execve().
        // Use just the filename now that we've chdir'd into its directory.
        std::vector<char*> argv;
        argv.push_back(const_cast<char*>(executableToRun.c_str()));
        argv.push_back(const_cast<char*>(scriptFile.c_str()));
        argv.push_back(nullptr);

        // Build the envp array for execve()
        std::vector<char*> envp;
        for (const std::string& e : env)
            envp.push_back(const_cast<char*>(e.c_str()));
        envp.push_back(nullptr);
        ::execve(executableToRun.c_str(), argv.data(), envp.data());
        // If execve() returns, something went wrong
        ::exit(1);
    }
    // PARENT PROCESS
    ::close(outPipe[1]); // we don't write to the CGI's stdout
    ::close(inPipe[0]);  // we don't read from the CGI's stdin

    _cgiFd = outPipe[0];
    _cgiStdinFd = inPipe[1];
    _cgiBody = body;
    _cgiBodyWritten = 0;

    // Pipes are blocking by default. Without this,
    // write()/read() here would freeze the whole event loop, even though
    // poll() told us the fd was ready, because a single write() call can
    // still block until the pipe has room for ALL the bytes we ask for.
    Net::setNonBlocking(_cgiFd);
    if (_cgiStdinFd != -1)
        Net::setNonBlocking(_cgiStdinFd);

    // If there is no body at all, close the write end right away
    // so the CGI sees EOF immediately instead of waiting forever.
    if (_cgiBody.empty())
    {
        ::close(_cgiStdinFd);
        _cgiStdinFd = -1;
    }
    // Record when this CGI process started, so EventLoop
    // can later detect if it has been running too long.
    _cgiStartTime = ::time(nullptr);
    _state = State::CGI;
}

// onCgiWritable() is called by EventLoop when the CGI stdin pipe is
// ready to accept more data. write() is non-blocking, so it only
// writes what currently fits, this never blocks the whole server.
void ClientConnection::onCgiWritable()
{
    if (_cgiStdinFd == -1)
        return; // nothing left to write

    // No need to cap this artificially, the pipe is non-blocking, so write()
    // only writes what fits and returns immediately either way. A small cap
    // just means far more poll() round trips than necessary.
    size_t remaining = _cgiBody.size() - _cgiBodyWritten;
    ssize_t written = ::write(_cgiStdinFd,
                            _cgiBody.data() + _cgiBodyWritten,
                            remaining);
    if (written > 0)
    {
        _cgiBodyWritten += static_cast<size_t>(written);

        if (_cgiBodyWritten >= _cgiBody.size())
        {
            // Entire body sent. Close the pipe so the CGI sees EOF,
            // exactly like it would after reading Content-Length bytes.
            ::close(_cgiStdinFd);
            _cgiStdinFd = -1;
            _cgiBody.clear();
        }
        return;
    }
    // written < 0: pipe buffer isn't ready right now. We don't inspect errno;
    // poll() already told us this fd was ready, and real fd errors/hangups
    // are caught separately via POLLERR/POLLHUP/POLLNVAL in EventLoop.
    return;
}

// Checks if this CGI script has been running longer than
// CGI_TIMEOUT_SECONDS. Called periodically by EventLoop for every
// connection in the CGI state.
bool ClientConnection::cgiTimedOut() const
{
    if (_state != State::CGI || _cgiPid == -1)
        return false;

    time_t now = ::time(nullptr);
    return (now - _cgiStartTime) >= CGI_TIMEOUT_SECONDS;
}

// Checks if this connection has gone quiet for too long, no bytes read
// or written, and hasn't sent or received anything in over
// IDLE_TIMEOUT_SECONDS. CGI connections are excluded here: they have
// their own timeout via cgiTimedOut(), a script legitimately running
// for a while shouldn't also count as an idle client.
bool ClientConnection::idleTimedOut() const
{
    if (_state == State::CGI)
        return false;

    time_t now = ::time(nullptr);
    return (now - _lastActivity) >= IDLE_TIMEOUT_SECONDS;
}

// Kills a hung CGI child, reaps it, closes both pipes, and queues
// a 504 Gateway Timeout response for the client.
void ClientConnection::killCgi()
{
    if (_cgiPid != -1)
    {
        ::kill(_cgiPid, SIGKILL);
        ::waitpid(_cgiPid, nullptr, 0); // blocking wait is fine here,
                                        // the child is already dying
        _cgiPid = -1;
    }

    if (_cgiFd != -1)
    {
        ::close(_cgiFd);
        _cgiFd = -1;
    }

    if (_cgiStdinFd != -1)
    {
        ::close(_cgiStdinFd);
        _cgiStdinFd = -1;
    }

    _cgiOutput.clear();
    _cgiBody.clear();

    queueResponse(HttpResponse::text(504, "Gateway Timeout"), false);
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
    if (bytesRead < 0)
    {
        _state = State::Closing;
        return; // Nothing to read right now; we don't inspect errno, poll()
                // will call us again. Genuine pipe errors surface as EOF
                // (bytesRead == 0) once the CGI process exits.
    }
      
    // bytesRead == 0 means the pipe closed, script is done
    ::close(_cgiFd);
    _cgiFd = -1;

    // The script may have exited without reading the whole request body
    // (very normal, e.g. a script that ignores POST data). If we were
    // still mid-write to its stdin, that pipe is now leaking: nothing
    // will ever read it again, since the CGI process is gone.
    if (_cgiStdinFd != -1)
    {
        ::close(_cgiStdinFd);
        _cgiStdinFd = -1;
        _cgiBody.clear();
    }

    int status = 0;
    bool cgiFailed = false;

    if (_cgiPid != -1)
    {
        ::waitpid(_cgiPid, &status, 0);
        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
            cgiFailed = true;
        _cgiPid = -1;
    }

    if (cgiFailed)
    {
        queueResponse(HttpResponse::text(502, "Bad Gateway"), false);
        _cgiOutput.clear();
        return;
    }

    size_t sep = _cgiOutput.find("\r\n\r\n");
    size_t offset = 4;
    if (sep == std::string::npos)
    {
        sep = _cgiOutput.find("\n\n");
        offset = 2;
    }

    HttpResponse resp;
    resp.status = 200;
    resp.reason = "OK";

    // Honor a CGI-provided "Status:" header line, if present.
    if (sep != std::string::npos)
    {
        size_t statusPos = _cgiOutput.find("Status:");
        if (statusPos != std::string::npos && statusPos < sep)
        {
            std::istringstream statusStream(_cgiOutput.substr(statusPos + 7));
            statusStream >> resp.status;
            std::getline(statusStream, resp.reason);
        }
    }

    if (sep != std::string::npos)
        resp.setBody(_cgiOutput.substr(sep + offset), "text/html; charset=utf-8");
    else
        resp.setBody(_cgiOutput, "text/html; charset=utf-8");

    _cgiOutput.clear();
    queueResponse(resp, false);
}
