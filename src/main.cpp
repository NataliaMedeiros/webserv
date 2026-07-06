#include "ServerManager.hpp"
#include <iostream>
#include <cstdlib>
#include "ConfigParser.hpp"
#include "Router.hpp"
#include "Handler.hpp"
#include "FileSystem.hpp"
#include <iostream>

// main() is the entry point of the server.
// Usage: ./webserv [port]
// If no port is given, we default to 8080.
//
// Example:
//   ./webserv        -> listens on localhost:8080
//   ./webserv 9090   -> listens on localhost:9090

#include "ConfigParser.hpp"
#include "ServerManager.hpp"
#include <iostream>
#include <vector>

// main() is the entry point of the server.
// Usage: ./webserv [config_file]
// If no config file is given, defaults to configs/default.config
//
// NOTE for Person 1: ConfigParser::parse() now returns a
// std::vector<ServerConfig> — one entry per "server { }" block in the
// config file. Each entry has its own .host and .port.
//
// Until ServerManager supports multiple listeners, this main only
// starts the FIRST server block (configs[0]). Once ServerManager can
// accept the full vector (one listener per entry, all sharing the
// same poll() set per subject rules), swap the loop below in.

int main(int argc, char** argv)
{
    std::string configPath = (argc >= 2) ? argv[1] : "configs/default.config";

    try
    {
        std::vector<ServerConfig> configs = ConfigParser::parse(configPath);

        std::cout << "Parsed " << configs.size() << " server block(s):\n";
        for (size_t i = 0; i < configs.size(); ++i)
        {
            std::cout << "  [" << i << "] " << configs[i].host
                       << ":" << configs[i].port
                       << " (root=" << configs[i].root << ")\n";
        }

        if (configs.empty())
            throw std::runtime_error("no server blocks found in config");

        // ── CURRENT: single-server startup (Person 1's existing ServerManager) ──
        std::cout << "\nStarting Webserv on port " << configs[0].port << "...\n";
        std::cout << "Open your browser at: http://localhost:" << configs[0].port << "/\n";

        ServerManager server(configs[0]);
        server.run(); // This never returns while the server is running

        // ── FUTURE: multi-server startup, once ServerManager supports it ──
        // for (size_t i = 0; i < configs.size(); ++i)
        //     serverManager.addListener(configs[i]);
        // serverManager.run();
    }
    catch (const std::exception& e)
    {
        std::cerr << "Fatal error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
