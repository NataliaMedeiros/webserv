#pragma once
#include <string>
#include <vector>
#include <map>

struct LocationConfig
{
    std::string              path;
    std::string              root;
    std::string              index;
    bool                     autoindex;
    std::string              uploadPath;
    std::string              cgiPass;
    std::string             cgiExtension;
    std::vector<std::string> methods;
    int                      redirectCode;
    std::string              redirectUrl;
    std::map<int, std::string> errorPages;
    size_t maxBodySize;
    bool   hasMaxBodySize;

    LocationConfig()
        : autoindex(false)
        , redirectCode(0)
        , maxBodySize(0)
        , hasMaxBodySize(false)
    {}
};
