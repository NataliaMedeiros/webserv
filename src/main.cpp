// #include "ServerManager.hpp"
// #include "ConfigParser.hpp"
// #include <iostream>
// #include <vector>

// int main(int argc, char** argv)
// {
//     std::string configPath = (argc >= 2) ? argv[1] : "configs/default.config";

//     try
//     {
//         std::vector<ServerConfig> configs = ConfigParser::parse(configPath);

//         if (configs.empty())
//             throw std::runtime_error("no server blocks found in config");

//         ServerManager server(configs);
//         server.run();
//     }
//     catch (const std::exception& e)
//     {
//         std::cerr << "Fatal error: " << e.what() << "\n";
//         return 1;
//     }

//     return 0;
// }
#include "ServerManager.hpp"
#include "ConfigParser.hpp"
#include <iostream>
#include <vector>
#include <csignal>

int main(int argc, char** argv)
{
    // Without this, writing to a pipe or socket whose other end already
    // closed (e.g. a CGI script that exits without reading all of its
    // stdin, or a client that disconnects mid-send) raises SIGPIPE, and
    // the default action for SIGPIPE is to kill the whole process.
    // Ignoring it means those writes just fail with -1/EPIPE instead,
    // which our I/O code already handles as "not ready / connection gone".
    ::signal(SIGPIPE, SIG_IGN);

    std::string configPath = (argc >= 2) ? argv[1] : "configs/default.config";

    try
    {
        std::vector<ServerConfig> configs = ConfigParser::parse(configPath);

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