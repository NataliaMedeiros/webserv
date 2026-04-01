#pragma once
#include "Listener.hpp"
#include "ConnectionStore.hpp"
#include <vector>
#include <poll.h>

// NOTE: EventLoop is the heart of the server.
// It monitors:
/// - listener fds for new connections
/// - client fds for readable/writable events
class EventLoop {
public:
  explicit EventLoop(Listener& listener, ConnectionStore& store);

  // Runs forever (MVP). In later phases you can add graceful shutdown.
  void run();

private:
  Listener& _listener;
  ConnectionStore& _store;

  std::vector<pollfd> _pollFds;

  void rebuildPollFds();
  void dispatchEvents();
};
