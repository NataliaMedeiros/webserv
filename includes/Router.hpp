#pragma once
#include "HttpRequest.hpp" // Friday 19 June Noor Added this include
#include "RouteDecision.hpp"
#include <string>
#include <vector>
#include "HttpResponse.hpp"
#include "ServerConfig.hpp"


// NOTE: Router decides which handler should run, based on request + (later) config.
class Router
{
    public:
        Router() : servConfig(nullptr) {}
        Router(const ServerConfig& servInput);
        // Returns the best-matching RouteDecision for this request.
        // Person 1 calls this with the parsed HttpRequest.
        RouteDecision route(const HttpRequest& req) const;

    private:
        const ServerConfig* servConfig;
};
