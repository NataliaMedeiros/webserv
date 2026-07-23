#pragma once
#include "Fd.hpp"
#include "HttpRequestParser.hpp"
#include "Router.hpp"
#include "Handler.hpp"
#include "ServerConfig.hpp"
#include <string>
#include <vector> // NEW
#include <poll.h>
#include <sys/types.h> // NEW: for pid_t

// NOTE: One ClientConnection represents one TCP client socket.
// It owns buffers and parsing state.
class ClientConnection {
public:
  // NEW: CGI state added (7 july, by Noor)
  // Reading --> waiting for a complete HTTP request
  // Writing --> sending response back to client
  // CGI --> waiting for a CGI child process to finish
  // Closing --> done, EventLoop will remove this connection
  enum class State { Reading, Writing, CGI, Closing };

  explicit ClientConnection(int fd, const ServerConfig& config);

  int fd() const { return _fd.get(); }

  // Which events should poll() watch for this client?
  // - We always watch READ unless closing.
  // - We watch WRITE only when we have data to send.
  short wantedEvents() const;

  // Called by EventLoop when fd is readable/writable.
  void onReadable();
  void onWritable();

  // EventLoop uses this to remove and close the client.
  bool shouldRemove() const;

  // NEW: CGI helpers used by EventLoop to add pipe fd to poll()
  bool hasCgiPipe() const { return _cgiFd != -1; }
  int cgiPipeFd() const { return _cgiFd; }
  void onCgiReadable(); // called by EventLoop when pipe has data

  // NEW 16 July: helpers for the CGI stdin write pipe
  bool hasCgiStdinPipe() const { return _cgiStdinFd != -1; }
  int cgiStdinFd() const { return _cgiStdinFd; }
  void onCgiWritable();

private:
  Fd _fd;
  State _state = State::Reading;

  // NOTE: Outgoing bytes waiting to be written.
  std::string _out;

  // Router decides which handler to use (static/upload/cgi).
  Router _router;

  // NOTE: Incremental parser that can accept partial reads.
  HttpRequestParser _parser; // before it wat RequestParser

  // NEW: CGI process tracking (7 july, Noor)
  // _cgiPid --> pid of the child process running the script
  // _cgiFd --> read end of the pipe, where CGI output comes from
  // _cgiOutput --> accumulated output from the CGI script
  pid_t _cgiPid = -1;
  int _cgiFd = -1;              // read end: CGI output comes from here
  std::string _cgiOutput;

  // NEW (16 july, by Noor): non-blocking write of the request body to CGI stdin
  int _cgiStdinFd = -1;         // write end: request body goes to the CGI's stdin
  std::string _cgiBody;         // the body still waiting to be written
  size_t _cgiBodyWritten = 0;   // how much of _cgiBody has been written so far

  // Build + queue a response for a complete request.
  void handleRequest(const HttpRequest& req);

  // Append serialized response to output buffer and update state.
  void queueResponse(const HttpResponse& resp, bool keepAlive);

  // NEW: starts a CGI child process (7 july, by Noor)
  void startCgi(const std::string& executable,
              const std::string& scriptPath,
              const std::vector<std::string>& env,
              const std::string& body);
};
