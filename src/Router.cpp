#include "Router.hpp"
#include <iostream>

// Router constructor.
// Stores a pointer to the ServerConfig so we can read locations during routing.
Router::Router(const ServerConfig& servInput) : servConfig(&servInput)
{}

static void copyServerDefaults(RouteDecision& decision, const ServerConfig& config)
{
    if (!config.root.empty())
        decision.root = config.root;
    if (!config.index.empty())
        decision.index = config.index;
    decision.errorPages = config.errorPages;
}

// route() — finds the best matching location for the request URI.
//
// Rules:
//   1. Location matches when URI starts with loc.path on a path boundary.
//   2. Longest matching location wins.
//   3. Location values override server values.
//   4. If no location matches, server/default values are still returned.
RouteDecision Router::route(const HttpRequest& req) const
{
    RouteDecision best;

    if (!servConfig)
        return best;

    copyServerDefaults(best, *servConfig);

    size_t bestLen = 0;

    for (std::vector<LocationConfig>::const_iterator it = servConfig->locations.begin();
         it != servConfig->locations.end(); ++it)
    {
        const LocationConfig& loc = *it;
        const std::string& locPath = loc.path;
        const std::string& uri = req.path;

        bool prefixMatch = uri.compare(0, locPath.size(), locPath) == 0;
        bool boundaryOk = (locPath == "/")
                       || (uri.size() == locPath.size())
                       || (uri[locPath.size()] == '/');

        if (prefixMatch && boundaryOk && locPath.size() > bestLen)
        {
            bestLen = locPath.size();
            best.locationPath = locPath;

            best.root = loc.root.empty() ? servConfig->root : loc.root;
            if (best.root.empty())
                best.root = "./www";

            if (!loc.index.empty())
                best.index = loc.index;

            best.autoindex = loc.autoindex;
            best.uploadPath = loc.uploadPath;
            best.cgiPass = loc.cgiPass;
            best.methods = loc.methods;
            best.redirectCode = loc.redirectCode;
            best.redirectUrl = loc.redirectUrl;

            best.errorPages = servConfig->errorPages;
            for (std::map<int, std::string>::const_iterator ep = loc.errorPages.begin();
                 ep != loc.errorPages.end(); ++ep)
            {
                best.errorPages[ep->first] = ep->second;
            }
        }
    }

    return best;
}
