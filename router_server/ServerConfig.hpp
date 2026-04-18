#pragma once
#include <string>
#include <vector>
#include "LocationConfig.hpp"

struct ServerConfig
{
    int                      port = 8080;
    std::string              root;
    std::string              index;
    std::vector<LocationConfig> locations;
};