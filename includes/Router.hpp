#pragma once
#include "Http.hpp"
#include "HttpRequest.hpp" // Friday 19 June Noor Added this include
#include "RouteDecision.hpp"


// NOTE: Router decides which handler should run, based on request + (later) config.
class Router {
public:
  RouteDecision route(const HttpRequest& req) const;

  // NOTE: future extension:
  // - pass Config reference into Router constructor
  // - match server_name + listen port + location blocks
};
