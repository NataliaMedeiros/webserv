#pragma once
#include "Listener.hpp"
#include "ConnectionStore.hpp"
#include "ServerConfig.hpp"
#include <map>
#include <vector>
#include <poll.h>

// NOTE: EventLoop is the heart of the server.
// It monitors:
/// - listener fds for new connections
/// - client fds for readable/writable events
class EventLoop {
public:
  // OLD
  //explicit EventLoop(Listener& listener, ConnectionStore& store);

  // NEW
  EventLoop(std::vector<Listener>& listeners,
            const std::vector<ServerConfig>& configs,
            ConnectionStore& store);
  // Runs forever (MVP). In later phases you can add graceful shutdown.
  void run();

private:
  // Listener& _listener;   OLD
  std::vector<Listener>& _listeners; // NEW:
  ConnectionStore& _store;
  std::vector<pollfd> _pollFds;
  std::map<int, ServerConfig> _listenerConfigs;

  void rebuildPollFds();
  void dispatchEvents();
};
