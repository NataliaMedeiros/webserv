#include "Fd.hpp"
void Fd::reset(int newFd) {
  if (_fd != -1) ::close(_fd);
  _fd = newFd;
}
