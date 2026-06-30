#pragma once
#include <string>
// #include <unordered_map>
// #include <string_view>

// // Friday 19th June, Noor deleted HttpRequest struct
// // Thalk with Noor about rename this file to the class name

// // NOTE: Response representation (structured).
// // Serialize with HttpResponse::toRaw().
// struct HttpResponse {
//   int status = 200;
//   std::string reason = "OK";
//   std::unordered_map<std::string, std::string> headers;
//   std::string body;

//   std::string toRaw(bool keepAlive) const;

//   static HttpResponse text(int status, std::string body);
//   static HttpResponse notFound();
//   static HttpResponse badRequest();
//   static HttpResponse methodNotAllowed();
// };


#include <map>

// Structured HTTP/1.1 response.
// Handler fills this object; ClientConnection serializes it with toRaw().
struct HttpResponse
{
    int status;
    std::string reason;
    std::map<std::string, std::string> headers;
    std::string body;

    HttpResponse();

    // Serialize to bytes ready for send().
    // keepAlive comes from HttpRequestParser / HttpRequest::keepAlive.
    std::string toRaw(bool keepAlive) const;

    // Convenience helpers used by handlers.
    void setHeader(const std::string& key, const std::string& value);
    bool hasHeader(const std::string& key) const;
    void setBody(const std::string& content, const std::string& contentType);

    static std::string reasonPhrase(int status);
    static HttpResponse text(int status, const std::string& body);
    static HttpResponse html(int status, const std::string& body);
    static HttpResponse error(int status, const std::string& message);

    static HttpResponse notFound();
    static HttpResponse badRequest();
    static HttpResponse methodNotAllowed();
};
