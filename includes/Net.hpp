#pragma once
#include <string>

namespace Net {
  // NOTE: Set an fd to non-blocking mode (required for event-driven server).
  bool setNonBlocking(int fd);

  // NOTE: For local dev it's helpful to set SO_REUSEADDR.
  bool setReuseAddr(int fd);

  // NOTE: Very small safety check against ../ path traversal.
  bool isSafeUrlPath(const std::string& urlPath);

  // NOTE: Minimal MIME guessing for static serving.
  std::string guessContentType(const std::string& path);

  // NOTE: Read a file fully into memory (MVP). Later you may stream large files.
  bool readFileToString(const std::string& path, std::string& out);
}
