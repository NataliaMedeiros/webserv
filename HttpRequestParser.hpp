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
		std::string toLower(std::string s);
		static std::string trim(const std::string& s);
		size_t findHeaderEnd(const std::string& raw);
		void splitHeaderBody(const std::string& raw, std::string& headerPart, std::string& bodyPart);
		std::vector<std::string> splitLines(const std::string& headerPart);
		void parseFirstLine(HttpRequest& request, const std::string& startLine);
		void parseHeaders(HttpRequest& request, const std::vector<std::string>& lines);
		bool parseBody(HttpRequest& request, const std::string& bodyPart);
		static bool isValidMethod(const std::string& method);
		static bool isValidVersion(const std::string& version);
		void splitPathQuery(HttpRequest& req);

	public:
		// bool parse(const std::string& raw, HttpRequest& request);
		enum Result
		{
			NeedMore,
			Complete,
			BadRequest
		};
		Result feed(const std::string& chunk, HttpRequest& req);
};

#endif

