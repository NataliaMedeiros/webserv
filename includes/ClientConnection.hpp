#pragma once
#include "Fd.hpp"
#include "RequestParser.hpp"
#include "Router.hpp"
#include "Handlers.hpp"
#include <string>
#include <poll.h>

// NOTE: One ClientConnection represents one TCP client socket.
// It owns buffers and parsing state.
class ClientConnection {
public:
  enum class State { Reading, Writing, Closing };

  explicit ClientConnection(int fd);

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

private:
  Fd _fd;
  State _state = State::Reading;

  // NOTE: Outgoing bytes waiting to be written.
  std::string _out;

  // NOTE: Incremental parser that can accept partial reads.
  RequestParser _parser;

  // Router decides which handler to use (static/upload/cgi).
  Router _router;

  // Build + queue a response for a complete request.
  void handleRequest(const HttpRequest& req);

  // Append serialized response to output buffer and update state.
  void queueResponse(const HttpResponse& resp, bool keepAlive);
};
