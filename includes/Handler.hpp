#pragma once
#include "Http.hpp"
#include "HttpRequest.hpp"
#include "RouteDecision.hpp"

// NOTE: In larger projects you can make a common interface:
// class IHandler { virtual HttpResponse handle(...) = 0; }
// For kickstart we keep it simple with free functions inside a namespace.
namespace Handlers {
  // Serve a file from disk (GET).
  HttpResponse serveStatic(const HttpRequest& req, const RouteDecision& d);

  // Handle upload (POST) - TODO for you to implement properly.
  HttpResponse handleUpload(const HttpRequest& req, const RouteDecision& d);

  // Run CGI (fork/exec/pipes) - TODO later.
  HttpResponse handleCgi(const HttpRequest& req, const RouteDecision& d);
}

// PLACEHOLDER: temporary Handler class, replace later with Sara's real
// Handler from router-standalone (she has her own RouteDecision and
// Http.hpp, those need to be merged too, same way we merged HttpRequest)
class Handler {
public:
  HttpResponse handle(const RouteDecision& d, const HttpRequest& req);
};