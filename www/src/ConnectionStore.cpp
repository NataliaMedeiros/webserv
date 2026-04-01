#include "ConnectionStore.hpp"

// ConnectionStore keeps track of all active client connections.
// It owns the ClientConnection objects - when a connection is removed,
// the object is destroyed and the fd is automatically closed (RAII).

// add() creates a new ClientConnection for the given fd and stores it.
void ConnectionStore::add(int fd)
{
    // unique_ptr means ConnectionStore owns this object.
    // When we call remove() or the store is destroyed, the ClientConnection
    // is automatically deleted and its fd is closed - no manual cleanup needed.
    _conns[fd] = std::unique_ptr<ClientConnection>(new ClientConnection(fd));
}

// remove() deletes the ClientConnection for the given fd.
// The unique_ptr destructor automatically closes the fd.
void ConnectionStore::remove(int fd)
{
    _conns.erase(fd);
}

// get() returns a pointer to the ClientConnection for the given fd.
// Returns nullptr if the fd is not in the store (connection already removed).
ClientConnection* ConnectionStore::get(int fd)
{
    std::map<int, std::unique_ptr<ClientConnection>>::iterator it = _conns.find(fd);
    if (it == _conns.end())
        return nullptr;
    return it->second.get();
}
