#pragma once
#include "Http.hpp"
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
