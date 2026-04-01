#pragma once
#include "ClientConnection.hpp"
#include <map>
#include <memory>

// NOTE: Keeps ownership of all connected clients.
// This avoids raw pointers and makes cleanup easy.
class ConnectionStore {
public:
  void add(int fd);
  void remove(int fd);
  ClientConnection* get(int fd);

  // Needed for iteration in EventLoop
  const std::map<int, std::unique_ptr<ClientConnection>>& all() const { return _conns; }

private:
  std::map<int, std::unique_ptr<ClientConnection>> _conns;
};
