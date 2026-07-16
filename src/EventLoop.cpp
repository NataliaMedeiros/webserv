#include "EventLoop.hpp"
#include <iostream>

// EventLoop is the heart of the server.
// It runs forever in a loop, doing three things each iteration:
//   1. Collect all open file descriptors (fds) into a list for poll()
//   2. Call poll() which waits until at least one fd has something to do
//   3. React to whichever fds are ready (new connection, data to read, or data to send)
//
// This is called an "event-driven" or "non-blocking" server:
// instead of waiting on one connection at a time, poll() watches ALL connections
// simultaneously and only acts when something is actually ready.
// 
// Multi port update (6 july by Noor)
// EventLoop now holds a vector of Listeners instead of just one.
// _listenerConfigs works like our memory: it maps each listener fd to its ServerConfig.
// We build it once in the constructor when we still know both the listener and its config.
// Later, when a new client comes in, we only see an fd number.
// Without this map we would no longer know which port it came from.
// and which config to give the new client.
EventLoop::EventLoop(std::vector<Listener>& listeners,
                    const std::vector<ServerConfig>& configs,
                    ConnectionStore& store)
    : _listeners(listeners), _store(store)
{
    for (size_t i = 0; i < listeners.size(); i++)
        _listenerConfigs[listeners[i].fd()] = configs [i];
}


// rebuildPollFds() builds the list of fds that poll() should watch.
// We rebuild it every iteration because connections are added and removed constantly.
void EventLoop::rebuildPollFds()
{
    _pollFds.clear();

    // Loop over all listeners instead of one,
    // so poll() watches every port we are listening on.
    for (auto& listener : _listeners)
    {
        pollfd listenerEntry;
        listenerEntry.fd      = listener.fd();
        listenerEntry.events  = POLLIN;
        listenerEntry.revents = 0;
        _pollFds.push_back(listenerEntry);
    }

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
    // CGI update (7 july, by Noor)
    // Also watch the pipe fd of any active CGI process, 
    // so poll() tells us when script output is ready to read
    for (auto& pair : _store.all())
    {
        ClientConnection* conn = pair.second.get();
        if (conn->hasCgiPipe())
        {
            pollfd cgiEntry;
            cgiEntry.fd = conn->cgiPipeFd();
            cgiEntry.events = POLLIN;
            cgiEntry.revents = 0;
            _pollFds.push_back(cgiEntry);
        }
    }
    // NEW (16 july, by Noor): also watch the CGI stdin pipe for writability,
    // so we can write the request body to the script without blocking.
    for (auto& pair : _store.all())
    {
        ClientConnection* conn = pair.second.get();
        if (conn->hasCgiStdinPipe())
        {
            pollfd cgiStdinEntry;
            cgiStdinEntry.fd      = conn->cgiStdinFd();
            cgiStdinEntry.events  = POLLOUT;
            cgiStdinEntry.revents = 0;
            _pollFds.push_back(cgiStdinEntry);
        }
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

        // Check if this fd belongs to one of our listeners (not a client).
        // We use _listenerConfigs as our memory to recognise listener fds.
        if (_listenerConfigs.count(p.fd)) // NEW "is the fd in our map?"
        {
            // Look up which config belongs to this listener fd
            const ServerConfig& config = _listenerConfigs.at(p.fd);

            // Find the matching listener so we can call acceptOne() on it
            Listener* activeListener = nullptr;
            for (auto& l : _listeners)
            {
               if (l.fd() == p.fd)
            {
                activeListener = &l;
                break;
            }
             }
            if (!activeListener)
                continue;

            // Accept all waiting clients in one go (there may be more than one)
            while (true)
            {
                int clientFd = activeListener->acceptOne();
                if (clientFd == -1)
                    break; // No more clients waiting right now

                // Pass the correct config so the client knows which server block it belongs to
                _store.add(clientFd, config);
                std::cout << "[+] New connection: fd=" << clientFd
                        << " on port " << config.port << "\n";
            }
            continue;
        }
        // CGI update (7 july, by Noor)
        // Check if this fd is a CGI pipe fd from one of our active connections.
        // We use _listenerConfigs as our memory to recognise listener fds,
        // so anything not in that map and not a client socket must be a CGI pipe.
        // --- Client fd: something happened on an existing connection ---
        bool isCgiPipe = false;
        ClientConnection* cgiConn = nullptr;
        for (auto& pair : _store.all())
        {
            if (pair.second->hasCgiPipe() && pair.second->cgiPipeFd() == p.fd)
            {
                isCgiPipe = true;
                cgiConn = pair.second.get();
                break;
            }
        }
        if (isCgiPipe)
        {
            if (cgiConn)
                cgiConn->onCgiReadable();
            continue;
        }
        // NEW (16 july, by Noor): check if this fd is a CGI stdin (write) pipe.
        bool isCgiStdinPipe = false;
        ClientConnection* cgiStdinConn = nullptr;
        for (auto& pair : _store.all())
        {
            if (pair.second->hasCgiStdinPipe() && pair.second->cgiStdinFd() == p.fd)
            {
                isCgiStdinPipe = true;
                cgiStdinConn = pair.second.get();
                break;
            }
        }
        if (isCgiStdinPipe)
        {
            if (cgiStdinConn)
                cgiStdinConn->onCgiWritable();
            continue;
        }
        // Client fd: something happened on an existing connection
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
