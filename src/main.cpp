#include "ServerManager.hpp"
#include <iostream>
#include <cstdlib>

// main() is the entry point of the server.
// Usage: ./webserv [port]
// If no port is given, we default to 8080.
//
// Example:
//   ./webserv        -> listens on localhost:8080
//   ./webserv 9090   -> listens on localhost:9090

int main(int argc, char** argv)
{
    // Read port from command line, or use 8080 as default
    uint16_t port = 8080;
    if (argc >= 2)
        port = static_cast<uint16_t>(std::atoi(argv[1]));

    // Wrap everything in try/catch so the server never crashes silently.
    // The subject requires the server to never terminate unexpectedly.
    try
    {
        std::cout << "Starting Webserv on port " << port << "...\n";
        std::cout << "Open your browser at: http://localhost:" << port << "/\n";

        ServerManager server(port);
        server.run(); // This never returns while the server is running
    }
    catch (const std::exception& e)
    {
        // Something went wrong during startup (e.g. port already in use)
        std::cerr << "Fatal error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
