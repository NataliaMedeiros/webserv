#ifndef HANDLER_HPP
#define HANDLER_HPP

#include "Router.hpp"
#include "Http.hpp"
#include "RouteDecision.hpp"
#include <string>

class Handler 
{
   public:
        HttpResponse handle(const RouteDecision& rd, const HttpRequest& req);

   private:
    
        HttpResponse handleStaticFile(const std::string& fullPath);
        HttpResponse handleRedirect(const RouteDecision& rd);
        HttpResponse handleDelete(const std::string& fullPath);

    // Helpers
        std::string  buildPath(const RouteDecision& rd, const HttpRequest& req);
        bool         isMethodAllowed(const RouteDecision& rd, const std::string& method);
        HttpResponse makeError(int code, const std::string& message);
};

#endif

