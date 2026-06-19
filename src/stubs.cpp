#include "Http.hpp"
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

// PLACEHOLDER: just calls the existing free Handlers:: functions,
// so we can test networking + parser without integrating Sara's
// branch yet. Replace once her Handler/Router/RouteDecision come in.
HttpResponse Handler::handle(const RouteDecision& d, const HttpRequest& req) {
    if (d.isCgi)
        return Handlers::handleCgi(req, d);
    if (req.method == "POST" && d.allowUpload)
        return Handlers::handleUpload(req, d);
    return Handlers::serveStatic(req, d);
}