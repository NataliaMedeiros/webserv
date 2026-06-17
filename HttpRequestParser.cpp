#include "HttpRequestParser.hpp"

#include <iostream>
#include <sstream>
#include <stdexcept>
#include <cctype>
#include <vector>
#include <cstdlib>

HttpRequestParser::Result HttpRequestParser::feed(const std::string& chunk, HttpRequest& req)
{
    _buf += chunk;

	// We need the full header section before we can do anything.
	if (findHeaderEnd(_buf) == std::string::npos)
		return NeedMore;

	// Work on a fresh request object so that partial data from a
	// previous NeedMore call never leaks into the final result.
	HttpRequest tmp;
	std::string headerPart, bodyPart;

	splitHeaderBody(_buf, headerPart, bodyPart);

	std::vector<std::string> lines = splitLines(headerPart);

	if (lines.empty())
		return BadRequest;

	try
	{
		parseFirstLine(tmp, lines[0]);
		parseHeaders(tmp, lines);
		if (!parseBody(tmp, bodyPart))
			return NeedMore;
	}
	catch (const std::exception&)
	{
		return BadRequest;
	}
	// Only write to the caller's req when the whole request is ready.
	req = tmp;
	// For now, clear everything after one request
	// Later you can consume only the used bytes
	_buf.clear();
	return Complete;
}

std::string HttpRequestParser::toLower(std::string s)
{
	for (size_t i = 0; i < s.size(); i++)
		s[i] = std::tolower(s[i]);
	return s;
}

// Remove leading and trailing whitespace/tabs/CR from a string.
std::string HttpRequestParser::trim(const std::string& s)
{
	size_t start = 0;
	while (start < s.size() &&
	       (s[start] == ' ' || s[start] == '\t' || s[start] == '\r'))
		++start;

	size_t end = s.size();
	while (end > start &&
	       (s[end - 1] == ' ' || s[end - 1] == '\t' || s[end - 1] == '\r'))
		--end;

	return s.substr(start, end - start);
}


size_t HttpRequestParser::findHeaderEnd(const std::string& raw)
{
	return raw.find("\r\n\r\n");
}

void HttpRequestParser::splitHeaderBody(const std::string& raw, std::string& headerPart, std::string& bodyPart)
{
	size_t pos = findHeaderEnd(raw);

	if (pos == std::string::npos)
		return;
	headerPart = raw.substr(0, pos);
	bodyPart = raw.substr(pos + 4);
}

std::vector<std::string> HttpRequestParser::splitLines(const std::string& headerPart)
{
	std::vector<std::string> lines; //Create a container to store each line separately
	size_t start = 0;

	while (true)
	{
		size_t end = headerPart.find("\r\n", start);
		if (end == std::string::npos)
		{ // it goes inside this just in case there's no more lines
			if (start < headerPart.size())
				lines.push_back(headerPart.substr(start));
			break;
		}
		lines.push_back(headerPart.substr(start, end - start));//Add line to the end of the vector lines
		start = end + 2; // skip "\r\n"
	}
	return lines;
}

void HttpRequestParser::parseFirstLine(HttpRequest& request, const std::string& startLine)
{
	size_t firstSpace = startLine.find(' ');
	if (firstSpace == std::string::npos)
		throw std::runtime_error("Invalid start line: missing first space in request line");
	size_t secondSpace = startLine.find(' ', firstSpace + 1);
	if (secondSpace == std::string::npos)
		throw std::runtime_error("Invalid start line: missing second space in request line");
	request.method = startLine.substr(0, firstSpace);
	request.path = startLine.substr(firstSpace + 1, secondSpace - firstSpace - 1);
	request.version = startLine.substr(secondSpace + 1);

	if (!isValidMethod(request.method))
		throw std::runtime_error("unsupported method: " + request.method);
	if (!isValidVersion(request.version))
		throw std::runtime_error("unsupported version: " + request.version);
	splitPathQuery(request);
}

// ===== Parse headers =====
//Entender melhor e checar se é a melhor maneira
void HttpRequestParser::parseHeaders(HttpRequest& request, const std::vector<std::string>& lines)
{
	for (size_t i = 1; i < lines.size(); i++) //headers starts at line 1
	{
		size_t sep = lines[i].find(":"); //: is the separator
		if (sep == std::string::npos)
			continue; //it will skip the line

		std::string key = toLower(trim(lines[i].substr(0, sep))); //before the separator
		std::string value = trim(lines[i].substr(sep + 1)); //after the separator

		request.headers[key] = value; //add or update the key value
	}
}

bool HttpRequestParser::parseBody(HttpRequest& request, const std::string& bodyPart)
{
	std::map<std::string, std::string>::const_iterator it = request.headers.find("content-length");
	if (it == request.headers.end())
	{
		request.body = "";
		return true;
	}
	std::istringstream iss(it->second);
	size_t length;
	iss >> length;
	if (iss.fail() || !iss.eof() || length < 0)
		throw std::runtime_error("Invalid Content-Length");
	if ((size_t)bodyPart.size() < length)
		return false;
	request.body = bodyPart.substr(0, length);
	return true;
}

bool HttpRequestParser::isValidMethod(const std::string& method)
{
	return method == "GET" || method == "POST" || method == "DELETE";
}

bool HttpRequestParser::isValidVersion(const std::string& version)
{
	return version == "HTTP/1.1";
}

void HttpRequestParser::splitPathQuery(HttpRequest& req)
{
	size_t qmark = req.path.find('?');
	if (qmark == std::string::npos)
		return; // no query string — nothing to split

	req.query_string = req.path.substr(qmark + 1);
	req.path         = req.path.substr(0, qmark);
}

