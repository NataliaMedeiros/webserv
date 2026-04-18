#include <string>
#include <vector>

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