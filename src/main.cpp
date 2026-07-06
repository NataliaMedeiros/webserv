#include "ServerManager.hpp"
#include "ConfigParser.hpp"
#include <iostream>
#include <vector>

// main() is the entry point of the server.
// Usage: ./webserv [config_file]
// If no config file is given, defaults to configs/default.config
//
// Multi port update (6 july, by Noor):
// ConfigParser::parse() returns a vector of ServerConfig, one per server block.
// ServerManager now takes the full vector and creates one Listener per config,
// all sharing the same poll() event loop.
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

        ServerManager server(configs);
        server.run();
    }
    catch (const std::exception& e)
    {
        std::cerr << "Fatal error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
