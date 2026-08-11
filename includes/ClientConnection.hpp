#pragma once
#include "Fd.hpp"
#include "HttpRequestParser.hpp"
#include "Router.hpp"
#include "Handler.hpp"
#include "ServerConfig.hpp"
#include <string>
#include <vector>
#include <poll.h>
#include <sys/types.h> // for pid_t
#include <ctime>

// NOTE: One ClientConnection represents one TCP client socket.
// It owns buffers and parsing state.
class ClientConnection {
public:
  // Reading --> waiting for a complete HTTP request
  // Writing --> sending response back to client
  // CGI --> waiting for a CGI child process to finish
  // Closing --> done, EventLoop will remove this connection
  enum class State { Reading, Writing, CGI, Closing };

  explicit ClientConnection(int fd, const ServerConfig& config);

  // Safety net: if connection is destroyed while CGI script is
  // still running (e.g. the browser dropped the connection abruptly),
  // this makes sure the child process is killed and reaped, and both
  // pipes are closed, no matter which code path caused the removal.
  ~ClientConnection();
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

  // CGI helpers used by EventLoop to add pipe fd to poll()
  bool hasCgiPipe() const { return _cgiFd != -1; }
  int cgiPipeFd() const { return _cgiFd; }
  void onCgiReadable(); // called by EventLoop when pipe has data

  // Helpers for the CGI stdin write pipe
  bool hasCgiStdinPipe() const { return _cgiStdinFd != -1; }
  int cgiStdinFd() const { return _cgiStdinFd; }
  void onCgiWritable();
  
  // Called periodically by EventLoop to check for a hung CGI script
  bool cgiTimedOut() const;
  void killCgi(); // kills and cleans up a timed-out CGI child, sends 504

  // Called periodically by EventLoop to check for a client that has
  // gone quiet, e.g. connected and never sent anything, or finished a
  // keep-alive request and never came back. Without this a silent
  // client can hold its fd open forever, since poll() never reports
  // anything for a socket nothing happens on.
  bool idleTimedOut() const;

private:
  Fd _fd;
  State _state = State::Reading;

  std::string _out; // Outgoing bytes waiting to be written.
  Router _router; // Router decides which handler to use (static/upload/cgi).
  HttpRequestParser _parser; // Incremental parser, that can accept partial reads.

  // CGI process tracking
  // _cgiPid --> pid of the child process running the script
  // _cgiFd --> read end of the pipe, where CGI output comes from
  // _cgiOutput --> accumulated output from the CGI script
  pid_t _cgiPid = -1;
  int _cgiFd = -1;
  std::string _cgiOutput;

  // Timeout for a hanging CGI script. If a script runs
  // longer than CGI_TIMEOUT_SECONDS, we kill it and respond with 504.
  static const int CGI_TIMEOUT_SECONDS = 30;
  time_t _cgiStartTime = 0;

  // Timeout for an idle client connection. Updated every time we
  // actually read or write bytes on the socket. If nothing happens for
  // IDLE_TIMEOUT_SECONDS, EventLoop closes the connection.
  static const int IDLE_TIMEOUT_SECONDS = 60;
  time_t _lastActivity;

  // Non-blocking write of the request body to CGI stdin
  int _cgiStdinFd = -1;         // write end: request body goes to the CGI's stdin
  std::string _cgiBody;         // the body still waiting to be written
  size_t _cgiBodyWritten = 0;   // how much of _cgiBody has been written so far

  // Build + queue a response for a complete request.
  void handleRequest(const HttpRequest& req);

  // Append serialized response to output buffer and update state.
  void queueResponse(const HttpResponse& resp, bool keepAlive);

  // Starts a CGI child process
  void startCgi(const std::string& executable,
              const std::string& scriptPath,
              const std::vector<std::string>& env,
              const std::string& body);
};
