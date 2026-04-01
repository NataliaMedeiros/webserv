#pragma once
#include "Http.hpp"
#include <string>
#include <string_view>

#include <iostream>
#include <sstream>
#include <stdexcept>
#include <cctype>


// NOTE: Incremental HTTP request parser.
// Why incremental? Because TCP may split a single HTTP request across many recv() calls.
// class RequestParser {
// public:
//   enum class Result { NeedMore, Complete, BadRequest };

//   // Feed newly received bytes. Parser stores an internal buffer.
//   Result feed(std::string_view chunk, HttpRequest& out);

//   // Helpful for debugging: how many bytes are buffered waiting for completion?
//   size_t bufferedBytes() const { return _buf.size(); }

// private:
//   std::string _buf;

//   bool parseHeaderBlock(std::string_view headerBlock, HttpRequest& req);
//   static std::string trim(std::string_view s);
//   static bool istarts_with(std::string_view a, std::string_view prefix);
// };

class RequestParser
{
	private:
		std::string _buf;
		std::string toLower(std::string s);
		size_t findHeaderEnd(const std::string& raw);
		void splitHeaderBody(const std::string& raw, std::string& headerPart, std::string& bodyPart);
		std::vector<std::string> splitLines(const std::string& headerPart);
		void parseFirstLine(HttpRequest& request, const std::string& startLine);
		void parseHeaders(HttpRequest& request, const std::vector<std::string>& lines);
		bool parseBody(HttpRequest& request, const std::string& bodyPart);

	public:
		bool parse(const std::string& raw, HttpRequest& request);
		enum Result
		{
			NeedMore,
			Complete,
			BadRequest
		};
		Result feed(const std::string& chunk, HttpRequest& req);
};
