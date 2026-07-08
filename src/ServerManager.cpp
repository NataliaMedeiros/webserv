#include "ServerManager.hpp"
#include <iostream>

// ServerManager is the top-level orchestrator.
// It creates and owns all the major components and starts the server.
//
// Component overview:
//   _listeners  -> one Listener per server block, each opens a socket on its own port
//   _store      -> keeps track of all active client connections
//   _loop       -> runs the poll() event loop that drives everything
//
// Multi port update (6 july, by Noor):
// Instead of one Listener, we now create one per ServerConfig.
// buildListeners() is a static helper that does this before the
// initializer list runs, because we need _listeners ready before
// we can pass it to EventLoop.

// static helper: builds a Listener for each config
std::vector<Listener> ServerManager::buildListeners(const std::vector<ServerConfig>& configs)
{
    std::vector<Listener> listeners;
    for (const ServerConfig& config : configs)
    {
        listeners.emplace_back(static_cast<uint16_t>(config.port));
        std::cout << "Starting Webserv on port " << config.port << "...\n";
        std::cout << "Open your browser at: http://localhost:" << config.port << "/\n";
    }
    return listeners;
}

ServerManager::ServerManager(const std::vector<ServerConfig>& configs)
    : _configs(configs),
      _listeners(buildListeners(_configs)),
      _store(),
      _loop(_listeners, _configs, _store)
{
}

// run() starts the server. It never returns while the server is alive.
void ServerManager::run()
{
    _loop.run();
}