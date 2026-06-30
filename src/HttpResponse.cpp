#include "HttpResponse.hpp"

#include <cctype>
#include <sstream>

static std::string lowerCopy(const std::string& s)
{
    std::string out = s;
    for (size_t i = 0; i < out.size(); ++i)
        out[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(out[i])));
    return out;
}

HttpResponse::HttpResponse()
    : status(200)
    , reason("OK")
{}

std::string HttpResponse::reasonPhrase(int code)
{
    switch (code)
    {
        case 200: return "OK";
        case 201: return "Created";
        case 204: return "No Content";
        case 301: return "Moved Permanently";
        case 302: return "Found";
        case 303: return "See Other";
        case 307: return "Temporary Redirect";
        case 308: return "Permanent Redirect";
        case 400: return "Bad Request";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 413: return "Payload Too Large";
        case 500: return "Internal Server Error";
        case 501: return "Not Implemented";
        case 502: return "Bad Gateway";
        case 504: return "Gateway Timeout";
        case 505: return "HTTP Version Not Supported";
        default:  return "Unknown";
    }
}

bool HttpResponse::hasHeader(const std::string& key) const
{
    const std::string wanted = lowerCopy(key);
    for (std::map<std::string, std::string>::const_iterator it = headers.begin();
         it != headers.end(); ++it)
    {
        if (lowerCopy(it->first) == wanted)
            return true;
    }
    return false;
}

void HttpResponse::setHeader(const std::string& key, const std::string& value)
{
    const std::string wanted = lowerCopy(key);
    for (std::map<std::string, std::string>::iterator it = headers.begin();
         it != headers.end(); ++it)
    {
        if (lowerCopy(it->first) == wanted)
        {
            headers.erase(it);
            break;
        }
    }
    headers[key] = value;
}

void HttpResponse::setBody(const std::string& content, const std::string& contentType)
{
    body = content;
    if (!contentType.empty())
        setHeader("Content-Type", contentType);
}

std::string HttpResponse::toRaw(bool keepAlive) const
{
    std::map<std::string, std::string> finalHeaders = headers;
    std::string finalBody = body;

    // 204 and 304 responses must not carry a message body.
    if (status == 204 || status == 304)
        finalBody.clear();

    HttpResponse headerHelper;
    headerHelper.headers = finalHeaders;
    headerHelper.setHeader("Content-Length", std::to_string(finalBody.size()));
    headerHelper.setHeader("Connection", keepAlive ? "keep-alive" : "close");

    // Useful default for simple text/error responses where the handler forgot it.
    if (!finalBody.empty() && !headerHelper.hasHeader("Content-Type"))
        headerHelper.setHeader("Content-Type", "text/plain; charset=utf-8");

    if (!headerHelper.hasHeader("Server"))
        headerHelper.setHeader("Server", "webserv");

    const std::string finalReason = reason.empty() ? reasonPhrase(status) : reason;

    std::ostringstream raw;
    raw << "HTTP/1.1 " << status << ' ' << finalReason << "\r\n";

    for (std::map<std::string, std::string>::const_iterator it = headerHelper.headers.begin();
         it != headerHelper.headers.end(); ++it)
    {
        raw << it->first << ": " << it->second << "\r\n";
    }

    raw << "\r\n";
    raw << finalBody;
    return raw.str();
}

HttpResponse HttpResponse::text(int status, const std::string& body)
{
    HttpResponse r;
    r.status = status;
    r.reason = reasonPhrase(status);
    r.setBody(body, "text/plain; charset=utf-8");
    return r;
}

HttpResponse HttpResponse::html(int status, const std::string& body)
{
    HttpResponse r;
    r.status = status;
    r.reason = reasonPhrase(status);
    r.setBody(body, "text/html; charset=utf-8");
    return r;
}

HttpResponse HttpResponse::error(int status, const std::string& message)
{
    std::ostringstream body;
    body << "<!DOCTYPE html>\n"
         << "<html><head><title>" << status << ' ' << message << "</title></head>\n"
         << "<body><h1>" << status << ' ' << message << "</h1></body></html>\n";

    HttpResponse r = html(status, body.str());
    r.reason = message.empty() ? reasonPhrase(status) : message;
    return r;
}

HttpResponse HttpResponse::notFound()
{
    return error(404, "Not Found");
}

HttpResponse HttpResponse::badRequest()
{
    return error(400, "Bad Request");
}

HttpResponse HttpResponse::methodNotAllowed()
{
    return error(405, "Method Not Allowed");
}
