#include <string>
#include <vector>
#include <map>

// ── Config structs (will be replaced by Person 2's parser output) ── //Discuss in Meeting
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
    size_t                      maxBodySize = 1024 * 1024; // default 1MB
    
    LocationConfig()
        : autoindex(false)
        , redirectCode(0)
        , maxBodySize(1024 * 1024)
    {}
};