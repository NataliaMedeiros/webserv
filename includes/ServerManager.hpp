#pragma once
#include "Listener.hpp"
#include "ConnectionStore.hpp"
#include "EventLoop.hpp"
#include "ServerConfig.hpp"

// NOTE: Top-level orchestration.
// - creates Listener
// - holds ConnectionStore
// - runs EventLoop
class ServerManager {
public:
  // explicit ServerManager(uint16_t port);
  explicit ServerManager(const ServerConfig& config);

  void run();

private:
  ServerConfig _config;
  Listener _listener;
  ConnectionStore _store;
  EventLoop _loop;
};
