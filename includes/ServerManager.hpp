#pragma once
#include "Listener.hpp"
#include "ConnectionStore.hpp"
#include "EventLoop.hpp"

// NOTE: Top-level orchestration.
// - creates Listener
// - holds ConnectionStore
// - runs EventLoop
class ServerManager {
public:
  explicit ServerManager(uint16_t port);

  void run();

private:
  Listener _listener;
  ConnectionStore _store;
  EventLoop _loop;
};
