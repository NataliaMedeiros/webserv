#pragma once
#include <string>
#include <vector>
#include <map>
#include "LocationConfig.hpp"

struct ServerConfig
{
    std::string              host = "0.0.0.0";//set default host to 0.0.0.0 in case congig file doesn't specify a host
    int                      port = 8080;//set default port to 8080
    std::string              root = "./www";
    std::string              index = "index.html";
    std::vector<LocationConfig> locations;
    std::map<int, std::string>   errorPages;
    size_t                       maxBodySize = 1024 * 1024; // default 1MB
};

//for multiple ports, default host is set to 0.0.0.0 
// because we want to listen on all interfaces. 
//If user specifies --
// - a host - we will use that host for all ports. 
// -multiple hosts -- we will use the first one for all ports. 
// -no host -- we will use the default host