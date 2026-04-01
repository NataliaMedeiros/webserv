#include "Http.hpp"
#include "RequestParser.hpp"
#include "Router.hpp"
#include "Handlers.hpp"
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

// // --- RequestParser.cpp stub (Person 2) ---

// RequestParser::Result RequestParser::feed(const std::string& chunk, HttpRequest& out) {
//     _buf += chunk;

//     size_t pos = _buf.find("\r\n\r\n");
//     if (pos == std::string::npos)
//         return Result::NeedMore;

//     size_t lineEnd = _buf.find("\r\n");
//     std::string requestLine = _buf.substr(0, lineEnd);

//     size_t s1 = requestLine.find(' ');
//     size_t s2 = requestLine.find(' ', s1 + 1);
//     if (s1 == std::string::npos || s2 == std::string::npos)
//         return Result::BadRequest;

//     out.method  = requestLine.substr(0, s1);
//     out.path    = requestLine.substr(s1 + 1, s2 - s1 - 1);
//     out.version = requestLine.substr(s2 + 1);
//     out.keepAlive = true;

//     _buf.clear();
//     return Result::Complete;
// }

// --- Router.cpp stub (Person 3) ---

RouteDecision Router::route(const HttpRequest& req) const {
    (void)req;
    RouteDecision d;
    d.root = "./www";
    return d;
}

// --- Handlers.cpp stubs (Person 3) ---

HttpResponse Handlers::serveStatic(const HttpRequest& req, const RouteDecision& d) {
    (void)req;
    (void)d;

    // Read index.html from disk and return it
    std::string body;
    if (Net::readFileToString("./www/index.html", body))
    {
        HttpResponse r;
        r.status = 200;
        r.reason = "OK";
        r.headers["Content-Type"] = "text/html; charset=utf-8";
        r.body = body;
        return r;
    }

    // Fallback if file not found
    HttpResponse r;
    r.status = 404;
    r.reason = "Not Found";
    r.body = "<h1>404 - File not found</h1>";
    return r;
}

HttpResponse Handlers::handleUpload(const HttpRequest& req, const RouteDecision& d) {
    (void)req;
    (void)d;
    return HttpResponse::text(200, "Upload stub - not implemented yet");
}

HttpResponse Handlers::handleCgi(const HttpRequest& req, const RouteDecision& d) {
    (void)req;
    (void)d;
    return HttpResponse::text(200, "CGI stub - not implemented yet");
}
