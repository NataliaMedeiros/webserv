#pragma once
#include "Http.hpp"
#include "RouteDecision.hpp"

// NOTE: Router decides which handler should run, based on request + (later) config.
class Router {
public:
  RouteDecision route(const HttpRequest& req) const;

  // NOTE: future extension:
  // - pass Config reference into Router constructor
  // - match server_name + listen port + location blocks
};
