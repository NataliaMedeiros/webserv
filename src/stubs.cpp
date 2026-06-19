#include "HttpResponse.hpp"
#include "Router.hpp"
#include "Handler.hpp"
#include "RouteDecision.hpp"
#include "Net.hpp"
#include <string>

// =============================================================================
// STUBS - Temporary placeholders for Person 2 and Person 3 code
// These will be replaced by real implementations later.
// =============================================================================

// --- Http.cpp stubs (Person 2) ---

std::string HttpResponse::toRaw(bool keepAlive) const {
    std::string raw = "HTTP/1.1 " + std::to_string(status) + " " + reason + "\r\n";
    for (auto& [key, val] : headers)
        raw += key + ": " + val + "\r\n";
    raw += "Content-Length: " + std::to_string(body.size()) + "\r\n";
    raw += std::string("Connection: ") + (keepAlive ? "keep-alive" : "close") + "\r\n";
    raw += "\r\n";
    raw += body;
    return raw;
}

HttpResponse HttpResponse::text(int s, std::string b) {
    HttpResponse r;
    r.status = s;
    r.reason = "OK";
    r.body = b;
    return r;
}

HttpResponse HttpResponse::notFound() {
    HttpResponse r;
    r.status = 404;
    r.reason = "Not Found";
    r.body = "<h1>404 Not Found</h1>";
    return r;
}

HttpResponse HttpResponse::badRequest() {
    HttpResponse r;
    r.status = 400;
    r.reason = "Bad Request";
    r.body = "<h1>400 Bad Request</h1>";
    return r;
}

HttpResponse HttpResponse::methodNotAllowed() {
    HttpResponse r;
    r.status = 405;
    r.reason = "Method Not Allowed";
    r.body = "<h1>405 Method Not Allowed</h1>";
    return r;
}
