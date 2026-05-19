#pragma once
#include "Fd.hpp"
#include <cstdint>

// NOTE: Listener = one listening socket on a port.
// Accepts incoming TCP connections and returns new client fds.
class Listener {
public:
  explicit Listener(uint16_t port);

  int fd() const { return _fd.get(); }
  uint16_t port() const { return _port; }

  // Accept a single client (non-blocking). Returns -1 if none available.
  int acceptOne();

private:
  uint16_t _port;
  Fd _fd;

  void openAndBind();
};
