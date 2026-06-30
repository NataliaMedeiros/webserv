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

int main(int argc, char** argv)
{
    // Read port from command line, or use 8080 as default
    // uint16_t port = 8080;
    std::string configPath = (argc >= 2) ? argv[1] : "configs/default.config";
    // if (argc >= 2)
        // port = static_cast<uint16_t>(std::atoi(argv[1]));

    // Wrap everything in try/catch so the server never crashes silently.
    // The subject requires the server to never terminate unexpectedly.
    try
    {
        ServerConfig config = ConfigParser::parse(configPath);
        std::cout << "Starting Webserv on port " << config.port << "...\n";
        std::cout << "Open your browser at: http://localhost:" << config.port << "/\n";

        ServerManager server(config);
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


//Temporary main() for testing multiple ports and multiple server blocks in the config file.
// #include "ConfigParser.hpp"
// #include <iostream>

// int main(int argc, char* argv[])
// {
//     if (argc != 2)
//     {
//         std::cerr << "Usage: " << argv[0] << " <config_file>\n";
//         return 1;
//     }

//     try
//     {
//         std::vector<ServerConfig> configs = ConfigParser::parse(argv[1]);

//         std::cout << "Parsed " << configs.size() << " server block(s)\n";
//         for (size_t i = 0; i < configs.size(); ++i)
//         {
//             const ServerConfig& cfg = configs[i];
//             std::cout << "\n── Server " << i << " ──\n";
//             std::cout << "  host: " << cfg.host << "\n";
//             std::cout << "  port: " << cfg.port << "\n";
//             std::cout << "  root: " << cfg.root << "\n";
//             std::cout << "  locations: " << cfg.locations.size() << "\n";
//             for (const auto& loc : cfg.locations)
//                 std::cout << "    - " << loc.path << " (root=" << loc.root << ")\n";
//         }
//     }
//     catch (const std::exception& e)
//     {
//         std::cerr << "Config error: " << e.what() << "\n";
//         return 1;
//     }

//     return 0;
//}
