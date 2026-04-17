#pragma once
#include <string>
#include <unordered_map>
#include <string_view>

// NOTE: Parsed request representation.
// This is the output of RequestParser.
struct HttpRequest {
  std::string method;
  std::string path;
  std::string version;
  std::unordered_map<std::string, std::string> headers;
  std::string body;
  bool keepAlive = true; // HTTP/1.1 default (unless Connection: close)
};

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
