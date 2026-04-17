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


//BIGGER VERSION
// #pragma once
// #include "Router.hpp"
// #include "HttpResponse.hpp"
// class Handler
// {
// public:
//     // Main entry point. Person 1 will call this.
//     HttpResponse handle(const RouteDecision& rd, const HttpRequest& req);

//     // Build the filesystem path from root + request path.
//     // Kept public so main.cpp test can still call it directly.
//     std::string buildPath(const RouteDecision& rd, const HttpRequest& req);

// private:
//     // Returns true if method string is in the allowed methods list.
//     // Empty methods list = allow everything (no restriction configured).
//     bool isMethodAllowed(const RouteDecision& rd, const std::string& method);

//     // Case 1: location has a redirect configured
//     HttpResponse handleRedirect(const RouteDecision& rd);

//     // Case 2: serve a static file from disk
//     HttpResponse handleStaticFile(const std::string& fullPath);

//     // Case 3: generate an HTML directory listing
//     HttpResponse handleAutoindex(const std::string& dirPath, const std::string& uriPath);

//     // Case 4: CGI execution (stub — fills in later)
//     HttpResponse handleCgi(const RouteDecision& rd, const HttpRequest& req,
//                            const std::string& fullPath);

//     // Case 5: handle file upload (POST body → disk)
//     HttpResponse handleUpload(const RouteDecision& rd, const HttpRequest& req);

//     // Build a simple error response
//     HttpResponse makeError(int code, const std::string& message);
// };
