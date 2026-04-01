#include "ServerManager.hpp"

// ServerManager is the top-level orchestrator.
// It creates and owns all the major components and starts the server.
//
// Component overview:
//   _listener  -> creates the server socket and accepts new TCP connections
//   _store     -> keeps track of all active client connections
//   _loop      -> runs the poll() event loop that drives everything

ServerManager::ServerManager(uint16_t port)
    : _listener(port),          // Open server socket on this port
      _store(),                  // Start with empty connection store
      _loop(_listener, _store)   // Give EventLoop references to both
{
}

// run() starts the server. It never returns while the server is alive.
void ServerManager::run()
{
    _loop.run();
}
