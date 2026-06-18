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
    std::vector<std::string> methods;
    int                      redirectCode;
    std::string              redirectUrl;
    std::map<int, std::string> errorPages;

    LocationConfig()
        : autoindex(false)
        , redirectCode(0)
    {}
};