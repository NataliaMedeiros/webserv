#pragma once
#include <unistd.h>

// NOTE: RAII wrapper around a file descriptor.
// - No leaks: closes automatically.
// - Movable, not copyable.
class Fd {
  int _fd = -1;

public:
  Fd() = default;
  explicit Fd(int fd) : _fd(fd) {}

  Fd(const Fd&) = delete;
  Fd& operator=(const Fd&) = delete;

  Fd(Fd&& o) noexcept : _fd(o._fd) { o._fd = -1; }
  Fd& operator=(Fd&& o) noexcept {
    if (this != &o) { reset(); _fd = o._fd; o._fd = -1; }
    return *this;
  }

  ~Fd() { reset(); }

  int get() const { return _fd; }
  explicit operator bool() const { return _fd != -1; }

  void reset(int newFd = -1);
};
