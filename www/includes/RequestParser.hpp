#pragma once
#include "Http.hpp"
#include <string>
#include <string_view>

// NOTE: Incremental HTTP request parser.
// Why incremental? Because TCP may split a single HTTP request across many recv() calls.
class RequestParser {
public:
  enum class Result { NeedMore, Complete, BadRequest };

  // Feed newly received bytes. Parser stores an internal buffer.
  Result feed(std::string_view chunk, HttpRequest& out);

  // Helpful for debugging: how many bytes are buffered waiting for completion?
  size_t bufferedBytes() const { return _buf.size(); }

private:
  std::string _buf;

  bool parseHeaderBlock(std::string_view headerBlock, HttpRequest& req);
  static std::string trim(std::string_view s);
  static bool istarts_with(std::string_view a, std::string_view prefix);
};
