#pragma once
#include "RouteDecision.hpp"
#include <string>
#include <vector>
#include "Http.hpp"
#include "ServerConfig.hpp"

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
