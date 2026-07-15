#pragma once

#include "Fd.hpp"
#include "Handler.hpp"
#include "HttpRequestParser.hpp"
#include "Router.hpp"
#include "ServerConfig.hpp"

#include <poll.h>
#include <sys/types.h>

#include <string>
#include <vector>

class ClientConnection
{
public:
    enum class State
    {
        Reading,
        Writing,
        CGI,
        Closing
    };

    explicit ClientConnection(int fd, const ServerConfig& config);
    ~ClientConnection();

    int fd() const { return _fd.get(); }

    short wantedEvents() const;
    void onReadable();
    void onWritable();
    bool shouldRemove() const;

    bool hasCgiInputPipe() const;
    bool hasCgiOutputPipe() const;
    int cgiInputFd() const;
    int cgiOutputFd() const;

    void onCgiInputWritable();
    void onCgiOutputReadable();
    void checkCgiFinished();

private:
    Fd _fd;
    State _state;
    std::string _out;
    Router _router;
    HttpRequestParser _parser;

    pid_t _cgiPid;
    int _cgiInputFd;
    int _cgiOutputFd;

    std::string _cgiInput;
    std::string _cgiOutput;
    size_t _cgiInputOffset;

    bool _cgiInputClosed;
    bool _cgiOutputClosed;
    bool _cgiKeepAlive;
    bool _cgiSuppressBody;
    bool _cgiExited;
    int _cgiExitStatus;

    void handleRequest(HttpRequest&& request);

    void queueResponse(
        const HttpResponse& response,
        bool keepAlive,
        bool suppressBody = false
    );

    void startCgi(
        const RouteDecision& decision,
        HttpRequest&& request,
        const std::string& fullPath
    );

    void finishCgi();
    void failCgi(int status, const std::string& reason);
    void cleanupCgi();

    static bool setNonBlocking(int descriptor);

    std::string buildRequestPath(
        const RouteDecision& decision,
        const HttpRequest& request
    ) const;

    bool shouldUseCgi(
        const RouteDecision& decision,
        const std::string& fullPath
    ) const;

    bool isMethodAllowed(
        const RouteDecision& decision,
        const std::string& method
    ) const;

    std::string joinAllowedMethods(
        const RouteDecision& decision
    ) const;

    std::string toUpperCgiName(
        const std::string& headerName
    ) const;

    std::string lowerCopy(const std::string& value) const;
    std::string trimCopy(const std::string& value) const;

    std::vector<std::string> buildCgiEnvironment(
        const HttpRequest& request,
        const std::string& fullPath
    ) const;

    HttpResponse parseCgiOutput(
        const std::string& output
    ) const;
};



// #pragma once
// #include "Fd.hpp"
// #include "HttpRequestParser.hpp" // before it was: #include "RequestParser.hpp"
// #include "Router.hpp"
// #include "Handler.hpp"
// #include "ServerConfig.hpp"
// #include <string>
// #include <vector> // NEW
// #include <poll.h>
// #include <sys/types.h> // NEW: for pid_t

// // NOTE: One ClientConnection represents one TCP client socket.
// // It owns buffers and parsing state.
// class ClientConnection {
// public:
//   // NEW: CGI state added (7 july, by Noor)
//   // Reading --> waiting for a complete HTTP request
//   // Writing --> sending response back to client
//   // CGI --> waiting for a CGI child process to finish
//   // Closing --> done, EventLoop will remove this connection
//   enum class State { Reading, Writing, CGI, Closing };

//   explicit ClientConnection(int fd, const ServerConfig& config);

//   int fd() const { return _fd.get(); }

//   // Which events should poll() watch for this client?
//   // - We always watch READ unless closing.
//   // - We watch WRITE only when we have data to send.
//   short wantedEvents() const;

//   // Called by EventLoop when fd is readable/writable.
//   void onReadable();
//   void onWritable();

//   // EventLoop uses this to remove and close the client.
//   bool shouldRemove() const;

//   // NEW: CGI helpers used by EventLoop to add pipe fd to poll()
//   bool hasCgiPipe() const { return _cgiFd != -1; }
//   int cgiPipeFd() const { return _cgiFd; }
//   void onCgiReadable(); // called by EventLoop when pipe has data

// private:
//   Fd _fd;
//   State _state = State::Reading;

//   // NOTE: Outgoing bytes waiting to be written.
//   std::string _out;

//   // Router decides which handler to use (static/upload/cgi).
//   Router _router;

//   // NOTE: Incremental parser that can accept partial reads.
//   HttpRequestParser _parser; // before it wat RequestParser

//   // NEW: CGI process tracking (7 july, Noor)
//   // _cgiPid --> pid of the child process running the script
//   // _cgiFd --> read end of the pipe, where CGI output comes from
//   // _cgiOutput --> accumulated output from the CGI script
//   pid_t _cgiPid = -1;
//   // int _cgiFd = -1;
//   // std::string _cgiOutput;


// int _cgiInputFd = -1;   // servidor escreve o body aqui
// int _cgiOutputFd = -1;  // servidor lê a resposta aqui

// std::string _cgiInput;
// size_t _cgiInputOffset = 0;

// std::string _cgiOutput;

// bool _cgiKeepAlive = false;
// bool _cgiInputClosed = false;
// bool _cgiOutputClosed = false;

//   // Build + queue a response for a complete request.
//   void handleRequest(HttpRequest& req);

//   // Append serialized response to output buffer and update state.
//   // void queueResponse(const HttpResponse& resp, bool keepAlive);
//   void queueResponse(const HttpResponse& resp, bool keepAlive, bool suppressBody = false);

//   // NEW: starts a CGI child process (7 july, by Noor)
//   // void startCgi(const std::string& executable,
//   //             const std::string& scriptPath,
//   //             const std::vector<std::string>& env,
//   //             const std::string& body);

// void startCgi(
//     const RouteDecision& decision,
//     HttpRequest&& req,
//     const std::string& fullPath
// );

// void onCgiInputWritable();
// void onCgiOutputReadable();
// void checkCgiFinished();
// void finishCgi();
// void cleanupCgi();

// bool hasCgiInputPipe() const;
// bool hasCgiOutputPipe() const;

// int cgiInputFd() const;
// int cgiOutputFd() const;
// };
