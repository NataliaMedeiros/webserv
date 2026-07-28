#pragma once
#include <vector>
#include <string>
#include "HttpResponse.hpp"
#include "HttpRequest.hpp"
#include "RouteDecision.hpp"

class Handler
{
  public:
        HttpResponse handle(const RouteDecision& rd, const HttpRequest& req);

      // Builds CGI environment variables, used by ClientConnection's non-blocking startCgi()
        static std::vector<std::string> buildCgiEnv(const RouteDecision& rd, 
                                                    const HttpRequest& req,
                                                    const std::string& fullPath);
                                                    
        // Needed by ClientConnection to compute the CGI script path
        // before deciding whether to use non-blocking startCgi()
        static std::string buildPath(const RouteDecision& rd, const HttpRequest& req);
        // Needed by ClientConnection to check CGI extension match
        static bool  hasExtension(const std::string& path, const std::string& ext);
  private:
        HttpResponse handleStaticFile(const RouteDecision& rd,const std::string& fullPath);
        HttpResponse handleRedirect(const RouteDecision& rd);
        HttpResponse handleDelete(const RouteDecision& rd, const std::string& fullPath);
        HttpResponse handleUpload(const RouteDecision& rd, const HttpRequest& req);
        HttpResponse handleAutoindex(const std::string& dirPath, const std::string& uriPath);
        bool         isMethodAllowed(const RouteDecision& rd, const std::string& method);
        HttpResponse makeError(const RouteDecision& rd, int code, const std::string& message);
        std::string  joinAllowedMethods(const RouteDecision& rd) const;
        bool         parseMultipart(const HttpRequest& req, std::string& outFilename, std::string& outFileContent);

};

