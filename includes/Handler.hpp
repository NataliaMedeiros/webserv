#pragma once
#include <vector>
#include <string>
#include "HttpResponse.hpp"
#include "HttpRequest.hpp"
#include "RouteDecision.hpp"

// NOTE: In larger projects you can make a common interface:
// class IHandler { virtual HttpResponse handle(...) = 0; }
// For kickstart we keep it simple with free functions inside a namespace.
// namespace Handlers {
//   // Serve a file from disk (GET).
//   HttpResponse serveStatic(const HttpRequest& req, const RouteDecision& d);

//   // Handle upload (POST) - TODO for you to implement properly.
//   HttpResponse handleUpload(const HttpRequest& req, const RouteDecision& d);

//   // Run CGI (fork/exec/pipes) - TODO later.
//   HttpResponse handleCgi(const HttpRequest& req, const RouteDecision& d);
// }

class Handler
{
  public:
        HttpResponse handle(const RouteDecision& rd, const HttpRequest& req);

        // NEW 16 July: builds CGI environment variables, shared between
        // Handler's own handleCgi() and ClientConnection's non-blocking startCgi()
        static std::vector<std::string> buildCgiEnv(const RouteDecision& rd, 
                                                    const HttpRequest& req,
                                                    const std::string& fullPath);

        // NEW 16 July: needed by ClientConnection to compute the CGI script path
        // before deciding whether to use non-blocking startCgi()
        static std::string buildPath(const RouteDecision& rd, const HttpRequest& req);
        // NEW 16 July: needed by ClientConnection to check CGI extension match
        static bool  hasExtension(const std::string& path, const std::string& ext);
  private:
        HttpResponse handleStaticFile(const RouteDecision& rd,const std::string& fullPath);
        HttpResponse handleRedirect(const RouteDecision& rd);
        HttpResponse handleDelete(const RouteDecision& rd, const std::string& fullPath);
        HttpResponse handleUpload(const RouteDecision& rd, const HttpRequest& req);
        HttpResponse handleAutoindex(const std::string& dirPath, const std::string& uriPath);
        HttpResponse handleCgi(const RouteDecision& rd, const HttpRequest& req,
          const std::string& fullPath);
        // std::string  buildPath(const RouteDecision& rd, const HttpRequest& req);
        bool         isMethodAllowed(const RouteDecision& rd, const std::string& method);
        HttpResponse makeError(const RouteDecision& rd, int code, const std::string& message);
        std::string  joinAllowedMethods(const RouteDecision& rd) const;
        bool         parseMultipart(const HttpRequest& req, std::string& outFilename, std::string& outFileContent);

};

