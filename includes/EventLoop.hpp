#pragma once
#include "Listener.hpp"
#include "ConnectionStore.hpp"
#include "ServerConfig.hpp"
#include <map>
#include <vector>
#include <poll.h>

// NOTE: EventLoop is the heart of the server. It monitors:
// - listener fds for new connections
// - client fds for readable/writable events
class EventLoop 
{
  public:
    EventLoop(std::vector<Listener>& listeners,
              const std::vector<ServerConfig>& configs,
              ConnectionStore& store);
    // Runs forever, until the process is killed.
    void run();

  private:
    std::vector<Listener>& _listeners;
    ConnectionStore& _store;
    std::vector<pollfd> _pollFds;
    std::map<int, ServerConfig> _listenerConfigs;

    void rebuildPollFds();
    void dispatchEvents();

    // Checks every active connection for a CGI script that
    // has been running too long, kills and cleans it up if so.
    void checkCgiTimeouts();
};
