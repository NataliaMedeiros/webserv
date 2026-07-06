#include "ConnectionStore.hpp"

// Noor: I deleted the whole constructor here, because 
// ConnectionStore no longer holds a global ServerConfig.
// Each client now receives the correct config via add(fd, config),
// passed in by EventLoop which knows which listener the client came from.
// // ConnectionStore keeps track of all active client connections.
// // It owns the ClientConnection objects - when a connection is removed,
// // the object is destroyed and the fd is automatically closed (RAII).

// ConnectionStore::ConnectionStore(const ServerConfig& config) : _config(config)
// {

// }
// add() creates a new ClientConnection for the given fd and stores it.


void ConnectionStore::add(int fd, const ServerConfig& config)
{
    // unique_ptr means ConnectionStore owns this object.
    // When we call remove() or the store is destroyed, the ClientConnection
    // is automatically deleted and its fd is closed - no manual cleanup needed.
    _conns[fd] = std::unique_ptr<ClientConnection>(
        new ClientConnection(fd, config)
    );
}

// remove() deletes the ClientConnection for the given fd.
// The unique_ptr destructor automatically closes the fd.
void ConnectionStore::remove(int fd)
{
    _conns.erase(fd);
}

// NEW get function (6 july, by Noor)
ClientConnection* ConnectionStore::get(int fd)
{
    auto it = _conns.find(fd);
    if (it == _conns.end())
        return nullptr;
    return it->second.get();
}
