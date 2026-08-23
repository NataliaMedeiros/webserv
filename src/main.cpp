#include "ServerManager.hpp"
#include "ConfigParser.hpp"
#include <iostream>
#include <vector>
#include <csignal>

int main(int argc, char** argv)
{
    //SIGPIPE is raised when server writes to a socket that has been closed by the client. By default, this kills the process.
    //Ignoring SIGPIPE here to keep the server alive;
    ::signal(SIGPIPE, SIG_IGN);

    if (argc > 2)
    {
        std::cerr << "Too many arguments" << std::endl;
        return 1;
    }
    std::string configPath = (argc == 2) ? argv[1] : "configs/default.config";

    try
    {
        std::vector<ServerConfig> configs = ConfigParser::parse(configPath);

        if (configs.empty())
            throw std::runtime_error("no server blocks found in config");

        // Builds every member of ServerManager, in declaration order. Nothing is served yet.
        //   1. _configs   - copies the whole vector<ServerConfig>
        //   2. _listeners - one Listener per block: calls openAndBind() from Listener constructor that makes-
                                    //--socket, reuseaddr, non-blocking, bind, listen (throws if the port
                                    //is taken or the host is invalid)
        //   3. _store     - empty ConnectionStore, no clients yet
        //   4. _loop      - EventLoop keeps references + maps listener fd -> its config
        // After this line the sockets are open and the kernel queues incoming
        // connections, but nothing accepts them until startServer().
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
