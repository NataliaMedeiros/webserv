#ifndef HTTPREQUESTPARSER_HPP
#define HTTPREQUESTPARSER_HPP

#include "HttpRequest.hpp"
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <cctype>
#include <vector>

class HttpRequestParser
{
	private:
		std::string _buf;
		static std::string trim(const std::string& s);
		size_t findHeaderEnd(const std::string& raw);
		void splitHeaderBody(const std::string& raw, std::string& headerPart, std::string& bodyPart);
		std::vector<std::string> splitLines(const std::string& headerPart);
		void parseFirstLine(HttpRequest& request, const std::string& startLine);
		void parseHeaders(HttpRequest& request, const std::vector<std::string>& lines);
		bool parseBody(HttpRequest& req, const std::string& bodyPart, size_t& bodyBytesConsumed);
		static bool isValidMethod(const std::string& method);
		static bool isValidVersion(const std::string& version);
		void splitPathQuery(HttpRequest& req);
		static void parseQueryString(HttpRequest& req);
		static bool parseChunkedBody(HttpRequest& req, const std::string& bodyPart, size_t& bytesConsumed);

	public:
		// bool parse(const std::string& raw, HttpRequest& request);
		enum Result
		{
			NeedMore,
			Complete,
			BadRequest,
			PayloadTooLarge
		};

		// Maximum bytes we will buffer before giving up.
		// Protects against clients that send endless headers (slow loris).
		// The real limit comes from the config file (client_max_body_size).
		// This is a hard safety ceiling — Sara's config value should be lower.
		static const size_t MAX_BUFFER_SIZE = 8 * 1024 * 1024; // 8 MB
		static std::string toLower(std::string s); // Noor: moved it to public! and made static
		Result feed(const std::string& chunk, HttpRequest& req);
};

#endif

