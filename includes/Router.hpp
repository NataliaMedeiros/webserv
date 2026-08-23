#pragma once
#include "HttpRequest.hpp"
#include "RouteDecision.hpp"
#include <string>
#include <vector>
#include "HttpResponse.hpp"
#include "ServerConfig.hpp"
#include <cstddef>

class Router
{
    public:
        Router() : servConfig(nullptr) {}
        Router(const ServerConfig& servInput);
        // Returns the best-matching RouteDecision for this request.
        RouteDecision route(const HttpRequest& req) const;
        size_t maxBodySizeFor(const std::string& path) const;

    private:
        const ServerConfig* servConfig;
};
