#pragma once
#include "Listener.hpp"
#include "ConnectionStore.hpp"
#include "EventLoop.hpp"
#include "ServerConfig.hpp"
#include <vector> // NEW (Noor, 6 July)

// NOTE: Top-level orchestration.
// Creates one Listener per server block in the config file.
// Holds ConnectionStore and runs EventLoop.
// Multi port update (6 july, by Noor): now takes a vector of ServerConfigs
// instead of one, and creates one Listener per config.
class ServerManager {
public:
  explicit ServerManager(const std::vector<ServerConfig>& configs);

  void run();

private:
  std::vector<ServerConfig> _configs;
  std::vector<Listener> _listeners;
  ConnectionStore _store;
  EventLoop _loop;

  // Helper to build listeners from configs, used in initializer list
  static std::vector<Listener> buildListeners(const std::vector<ServerConfig>& configs);
};
