#pragma once
#include <string>
#include <vector>

// Output of Router. Tells Handler exactly what to do with this request.
// Every field maps directly to something in LocationConfig or ServerConfig.
struct RouteDecision
{
    std::string              root;         // filesystem root to serve from
    std::string              index;        // index filename for directory requests
    std::string              fullPath;     // computed by Handler::buildPath (not by Router)
    bool                     autoindex;    // generate directory listing if no index file?
    std::string              uploadPath;   // where to write uploaded files
    std::string              cgiPass;      // path to CGI executable (empty = no CGI)
    std::vector<std::string> methods;      // allowed methods (empty = allow all)
    int                      redirectCode; // 0 = no redirect, 301/302 etc = redirect
    std::string              redirectUrl;  // target URL for redirect (empty if no redirect)
    std::string              locationPath; // the matched location's path, e.g. "/images"

    RouteDecision()
        : root("./www")
        , index("index.html")
        , autoindex(false)
        , redirectCode(0)
    {}
};
