#include "ServerManager.hpp"
#include "ConfigParser.hpp"
#include <iostream>
#include <vector>
#include <csignal>

int main(int argc, char** argv)
{
    // SIGPIPE prevents the program to close when one client is disconnected
    ::signal(SIGPIPE, SIG_IGN);

    if (argc > 2)
    {
        std::cerr << "Too many arguments" << std::endl;
        return 0;
    }
    std::string configPath = (argc == 2) ? argv[1] : "configs/default.config";

    try
    {
        std::vector<ServerConfig> configs = ConfigParser::parse(configPath);

        if (configs.empty())
            throw std::runtime_error("no server blocks found in config");

        // 
        ServerManager server(configs);
        server.startServer();
    }
    catch (const std::exception& e)
    {
        std::cerr << "Fatal error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
