#pragma once
#include "RouteDecision.hpp"
#include <string>
#include <vector>
#include "Http.hpp"

// ── Config structs (will be replaced by Person 2's parser output) ──
struct LocationConfig
{
    std::string              path;
    std::string              root;
    std::string              index;
    bool                     autoindex;
    std::string              uploadPath;
    std::string              cgiPass;
    std::vector<std::string> methods;
    int                      redirectCode;
    std::string              redirectUrl;

    LocationConfig()
        : autoindex(false)
        , redirectCode(0)
    {}
};

struct ServerConfig
{
    std::string              root;
    std::string              index;
    std::vector<LocationConfig> locations;
};

class Router
{
    public:
        Router() : servConfig(nullptr) {}
        Router(const ServerConfig& servInput);
        // Returns the best-matching RouteDecision for this request.
        // Person 1 calls this with the parsed HttpRequest.
        RouteDecision route(const HttpRequest& req) const;

    private:
        const ServerConfig* servConfig;
};
