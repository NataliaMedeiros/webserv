#include "Router.hpp"

// Router constructor.
// Stores a pointer to the ServerConfig so we can read locations during routing.
// We take a const reference (not a copy) because the config is owned elsewhere.
Router::Router(const ServerConfig& servInput) : servConfig(&servInput)
{}

// route() — finds the best matching location for the request URI.
//
// Rules (in order):
//   1. A location matches if the URI starts with its path AND
//      the match is on a path boundary (exact match OR followed by '/').
//      This prevents /images from matching /imageset.
//   2. Among all matches, the one with the longest path wins.
//   3. If the matched location has an empty root, fall back to server root.
//   4. If no location matches at all, return a default RouteDecision
//      (which uses the server root set in the RouteDecision constructor).
RouteDecision Router::route(const HttpRequest& req) const
{
    if (!servConfig)
        return RouteDecision();

    RouteDecision best;
    size_t        bestLen = 0;
    bool          found   = false;

    for (const LocationConfig& loc : servConfig->locations)
    {
        const std::string& locPath = loc.path;
        const std::string& uri     = req.path;

        // Boundary-aware prefix match:
        // uri must start with locPath AND the next character (if any) must be '/'
        // (or there is no next character — exact match).
        bool prefixMatch = uri.compare(0, locPath.size(), locPath) == 0;
        bool boundaryOk  = (locPath == "/")
                        || (uri.size() == locPath.size())
                        || (uri[locPath.size()] == '/');

        if (prefixMatch && boundaryOk)
        {
            if (locPath.size() > bestLen)
            {
                bestLen = locPath.size();
                found   = true;
                best.locationPath = locPath;//store the matched location path in the RouteDecision for later use by Handler
                // Fill from location — fall back to server root if location has none
                best.root = loc.root.empty() ? servConfig->root : loc.root;

                // Index: use location value if set, otherwise keep RouteDecision default
                if (!loc.index.empty())
                    best.index = loc.index;

                best.autoindex   = loc.autoindex;
                best.uploadPath  = loc.uploadPath;
                best.cgiPass     = loc.cgiPass;
                best.methods     = loc.methods;
                best.redirectCode = loc.redirectCode;
                best.redirectUrl  = loc.redirectUrl;
            }
        }
    }

    // No location matched — return defaults (server root from RouteDecision constructor)
    if (!found)
    {
        RouteDecision fallback;
        fallback.root = servConfig->root;
        return fallback;
    }

    return best;
}
