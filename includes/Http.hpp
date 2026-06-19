#pragma once
#include <string>
#include <unordered_map>
#include <string_view>

// Friday 19th June, Noor deleted HttpRequest struct

// NOTE: Response representation (structured).
// Serialize with HttpResponse::toRaw().
struct HttpResponse {
  int status = 200;
  std::string reason = "OK";
  std::unordered_map<std::string, std::string> headers;
  std::string body;

  std::string toRaw(bool keepAlive) const;

  static HttpResponse text(int status, std::string body);
  static HttpResponse notFound();
  static HttpResponse badRequest();
  static HttpResponse methodNotAllowed();
};
