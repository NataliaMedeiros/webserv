#ifndef HTTPREQUESTPARSER_HPP
#define HTTPREQUESTPARSER_HPP

#include "HttpRequest.hpp"
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <cctype>

class HttpRequestParser
{
	public:
		bool parse(const std::string& raw, HttpRequest& request);

	private:
		std::string toLower(std::string s);
		size_t findHeaderEnd(const std::string& raw);
		void splitHeaderBody(const std::string& raw, std::string& headerPart, std::string& bodyPart);
		std::vector<std::string> splitLines(const std::string& headerPart);
		std::vector<std::string> splitLinesManual(const std::string& headerPart);
		void parseFirstLine(HttpRequest& request, const std::string& startLine);
		void parseHeaders(HttpRequest& request, const std::vector<std::string>& lines);
		bool parseBody(HttpRequest& request, const std::string& bodyPart);
};

#endif

