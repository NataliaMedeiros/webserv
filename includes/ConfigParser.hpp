#pragma once
#include "ServerConfig.hpp"
#include <string>
#include <vector>

class ConfigParser
{
public:
   
    //Returns one ServerConfig per "server{ ... }" block in the config file.
    static std::vector<ServerConfig> parse(const std::string& filename);

private:
    // Splits the whole file text into tokens like: "server", "{", "listen", "8080", ";", "}"
    static std::vector<std::string> tokenize(const std::string& text);

    // Walks through tokens and fills the struct.
    static ServerConfig parseServer(std::vector<std::string>& tokens, size_t& i);
    static LocationConfig parseLocation(std::vector<std::string>& tokens, size_t& i);
};