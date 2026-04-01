#include "Net.hpp"
#include <fcntl.h>
#include <sys/socket.h>
#include <fstream>

bool Net::setNonBlocking(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags < 0) return false;
  return fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
}

bool Net::setReuseAddr(int fd) {
  int yes = 1;
  return setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == 0;
}

bool Net::isSafeUrlPath(const std::string& p) {
  return p.find("..") == std::string::npos && p.find('\0') == std::string::npos;
}

std::string Net::guessContentType(const std::string& path) {
  size_t len = path.size();
  if (len >= 5 && path.substr(len - 5) == ".html") return "text/html; charset=utf-8";
  if (len >= 4 && path.substr(len - 4) == ".css")  return "text/css; charset=utf-8";
  if (len >= 3 && path.substr(len - 3) == ".js")   return "application/javascript";
  if (len >= 4 && path.substr(len - 4) == ".png")  return "image/png";
  if (len >= 4 && path.substr(len - 4) == ".jpg")  return "image/jpeg";
  if (len >= 5 && path.substr(len - 5) == ".jpeg") return "image/jpeg";
  if (len >= 4 && path.substr(len - 4) == ".txt")  return "text/plain; charset=utf-8";
  return "application/octet-stream";
}

bool Net::readFileToString(const std::string& path, std::string& out) {
  std::ifstream in(path, std::ios::binary);
  if (!in) return false;
  out.assign((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  return true;
}
