#include "Listener.hpp"
#include "Net.hpp"
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdexcept>
#include <cerrno>
#include <cstring>

// Listener is responsible for the server's "front door".
// It creates a TCP socket, binds it to a port, and listens for incoming connections.
// When a new browser connects, acceptOne() returns a new file descriptor (fd)
// for that specific connection.
Listener::Listener(uint16_t port, const std::string& host) : _port(port), _host(host)
{
    openAndBind();
}

void Listener::openAndBind()
{
    // Step 1: Create a TCP socket.
    // AF_INET    = IPv4 internet protocol
    // SOCK_STREAM = TCP (reliable, connection-based)
    // The result is a file descriptor (fd) - just a number the OS uses to track this socket.
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
        throw std::runtime_error("socket() failed: could not create server socket");

    // Store the fd in our RAII wrapper (Fd class closes it automatically on destruction)
    _fd.reset(fd);

    // Step 2: Allow reusing the port immediately after the server restarts.
    // Without this, you get "address already in use" errors during development.
    Net::setReuseAddr(_fd.get());

    // Step 3: Set the socket to non-blocking mode.
    // This means accept() will return immediately even if no client is waiting,
    // instead of freezing the server until someone connects.
    Net::setNonBlocking(_fd.get());

// Step 4: Bind - attach the socket to a specific port on this machine.
    // NEW (Noor): use the configured host/interface via inet_pton() instead
    // of always INADDR_ANY. "0.0.0.0" (the default) still means "all
    // interfaces", inet_pton() correctly parses it to the same value.
    // A genuinely invalid host string now fails loudly instead of being
    // silently ignored.
    // htons() converts the port number to network byte order (big-endian)
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(_port);

    if (::inet_pton(AF_INET, _host.c_str(), &addr.sin_addr) != 1)
        throw std::runtime_error("invalid host address: " + _host);

    if (::bind(_fd.get(), (sockaddr*)&addr, sizeof(addr)) < 0)
        throw std::runtime_error(std::string("bind() failed: ") + std::strerror(errno));

    // Step 5: Listen - mark the socket as passive, ready to accept connections.
    // SOMAXCONN = maximum number of pending connections in the queue (system default)
    if (::listen(_fd.get(), SOMAXCONN) < 0)
        throw std::runtime_error("listen() failed");
}

// acceptOne() accepts one incoming client connection.
// Returns the new client fd on success, or -1 if no client is waiting right now.
// Because the socket is non-blocking, -1 just means "try again later" - not an error.
int Listener::acceptOne()
{
    int clientFd = ::accept(_fd.get(), nullptr, nullptr);
    if (clientFd < 0)
        return -1;  // No client waiting right now, or an error occurred.
                // We don't inspect errno (per subject rules); either way,
                // there is simply nothing to accept at this moment.
    // Make the new client connection non-blocking too,
    // so recv() and send() never freeze the server.
    Net::setNonBlocking(clientFd);
    return clientFd;
}
