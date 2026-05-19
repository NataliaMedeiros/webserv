#include "EventLoop.hpp"
#include <iostream>

// EventLoop is the heart of the server.
// It runs forever in a loop, doing three things each iteration:
//   1. Collect all open file descriptors (fds) into a list for poll()
//   2. Call poll() - which waits until at least one fd has something to do
//   3. React to whichever fds are ready (new connection, data to read, or data to send)
//
// This is called an "event-driven" or "non-blocking" server:
// instead of waiting on one connection at a time, poll() watches ALL connections
// simultaneously and only acts when something is actually ready.

EventLoop::EventLoop(Listener& listener, ConnectionStore& store)
    : _listener(listener), _store(store)
{
}

// rebuildPollFds() builds the list of fds that poll() should watch.
// We rebuild it every iteration because connections are added and removed constantly.
void EventLoop::rebuildPollFds()
{
    _pollFds.clear();

    // Always watch the listener fd for new incoming connections (POLLIN = readable)
    pollfd listenerEntry;
    listenerEntry.fd      = _listener.fd();
    listenerEntry.events  = POLLIN;
    listenerEntry.revents = 0;
    _pollFds.push_back(listenerEntry);

    // Add every active client connection with the events it is interested in.
    // Each ClientConnection tells us what it wants via wantedEvents():
    //   POLLIN  = "I want to read incoming data"
    //   POLLOUT = "I have data ready to send"
    for (auto& pair : _store.all())
    {
        ClientConnection* conn = pair.second.get();
        pollfd clientEntry;
        clientEntry.fd      = pair.first;
        clientEntry.events  = conn->wantedEvents();
        clientEntry.revents = 0;
        _pollFds.push_back(clientEntry);
    }
}

// dispatchEvents() checks what poll() found and calls the right handler.
void EventLoop::dispatchEvents()
{
    for (size_t i = 0; i < _pollFds.size(); i++)
    {
        pollfd& p = _pollFds[i];

        // revents == 0 means nothing happened on this fd - skip it
        if (p.revents == 0)
            continue;

        // --- Listener fd: a new client wants to connect ---
        if (p.fd == _listener.fd())
        {
            // Accept all waiting clients in one go (there may be more than one)
            while (true)
            {
                int clientFd = _listener.acceptOne();
                if (clientFd == -1)
                    break; // No more clients waiting right now

                _store.add(clientFd);
                std::cout << "[+] New connection: fd=" << clientFd << "\n";
            }
            continue;
        }

        // --- Client fd: something happened on an existing connection ---
        ClientConnection* conn = _store.get(p.fd);
        if (!conn)
            continue; // Connection was already removed this iteration - skip

        // Handle errors and disconnects (browser closed the tab, network dropped, etc.)
        if (p.revents & (POLLERR | POLLHUP | POLLNVAL))
        {
            _store.remove(p.fd);
            std::cout << "[-] Connection closed: fd=" << p.fd << " (error or hangup)\n";
            continue;
        }

        // POLLIN: the client sent us data - read and parse it
        if (p.revents & POLLIN)
            conn->onReadable();

        // POLLOUT: the socket is ready to send - flush our response buffer
        if (p.revents & POLLOUT)
            conn->onWritable();

        // Check if this connection is done (state == Closing and nothing left to send)
        if (conn->shouldRemove())
        {
            _store.remove(p.fd);
            std::cout << "[-] Connection done: fd=" << p.fd << "\n";
        }
    }
}

// run() is the main server loop - it never returns while the server is alive.
void EventLoop::run()
{
    std::cout << "Event loop started. Waiting for connections...\n";

    while (true)
    {
        // Step 1: build the list of fds to watch
        rebuildPollFds();

        // Step 2: wait until at least one fd is ready
        // -1 as timeout means "wait forever" - poll() blocks until something happens
        int readyCount = ::poll(_pollFds.data(), _pollFds.size(), -1);

        // If poll() was interrupted (e.g. by a signal), just try again
        if (readyCount < 0)
            continue;

        // Step 3: handle all ready fds
        dispatchEvents();
    }
}
