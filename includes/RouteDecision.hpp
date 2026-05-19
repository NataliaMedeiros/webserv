#pragma once
#include <string>
#include <vector>

// NOTE: Output of Router: tells server *what* to do with a request.
struct RouteDecision {
  std::string root = "./www";       // filesystem root for static content
  std::string resolvedPath;         // filled by Router/Handler
  bool isCgi = false;
  bool allowUpload = false;
  std::string uploadDir = "./uploads";
  std::vector<std::string> allowedMethods = {"GET", "POST", "DELETE"};
};
