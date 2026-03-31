#include "HttpRequestParser.hpp"

#include <iostream>
#include <sstream>
#include <stdexcept>
#include <cctype>
#include <vector>

HttpRequestParser::Result HttpRequestParser::feed(const std::string& chunk, HttpRequest& req)
{
    _buf += chunk;

    // Reset request before parsing
    req.method.clear();
    req.path.clear();
    req.version.clear();
    req.headers.clear();
    req.body.clear();

	size_t pos = findHeaderEnd(_buf);
	if (pos == std::string::npos)
		return HttpRequestParser::NeedMore;
	std::string headerPart;
	std::string bodyPart;
	splitHeaderBody(_buf, headerPart, bodyPart);
	std::vector<std::string> lines = splitLines(headerPart);
	if (lines.empty())
		return HttpRequestParser::BadRequest;
	try
	{
		parseFirstLine(req, lines[0]);
		parseHeaders(req, lines);
		if (!parseBody(req, bodyPart))
			return HttpRequestParser::NeedMore;
	}
	catch (const std::exception&)
	{
		return HttpRequestParser::BadRequest;
	}
	// For now, clear everything after one request
	// Later you can consume only the used bytes
	_buf.clear();
	return HttpRequestParser::Complete;
}

bool HttpRequestParser::parse(const std::string& raw, HttpRequest& request)
{
	std::string headerPart;
	std::string bodyPart;
	std::vector<std::string> lines;

	size_t pos = findHeaderEnd(raw);
	if (pos == std::string::npos)
		return false;
	splitHeaderBody(raw, headerPart, bodyPart);
	lines = splitLines(headerPart);
	if (lines.empty())
		throw std::runtime_error("Empty request");
	parseFirstLine(request, lines[0]);
	parseHeaders(request, lines);
	if (!parseBody(request, bodyPart))
		return false;
	return true;
}

std::string HttpRequestParser::toLower(std::string s)
{
	for (size_t i = 0; i < s.size(); i++)
		s[i] = std::tolower(s[i]);
	return s;
}

size_t HttpRequestParser::findHeaderEnd(const std::string& raw)
{
	size_t pos = raw.find("\r\n\r\n");

	if (pos == std::string::npos) //npos is "not found". Its value is usually the largest possible size_t (-1 under the hood).
	{
		std::cout << "Headers not complete yet\n";
		return std::string::npos;
	}
	return pos;
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
		{ // it goes inside this if just in case there's no more lines
			if (start < headerPart.size())
				lines.push_back(headerPart.substr(start));
			break;
		}
		std::string line = headerPart.substr(start, end - start);
		lines.push_back(line);//Add line to the end of the vector lines
		start = end + 2; // skip "\r\n"
	}
	return lines;
}

void HttpRequestParser::parseFirstLine(HttpRequest& request, const std::string& startLine)
{
	size_t firstSpace = startLine.find(' ');
	if (firstSpace == std::string::npos)
		throw std::runtime_error("Invalid start line");
	size_t secondSpace = startLine.find(' ', firstSpace + 1);
	if (secondSpace == std::string::npos)
		throw std::runtime_error("Invalid start line");
	request.method = startLine.substr(0, firstSpace);
	request.path = startLine.substr(firstSpace + 1, secondSpace - firstSpace - 1);
	request.version = startLine.substr(secondSpace + 1);
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
		std::string key = lines[i].substr(0, sep); //before the separator
		std::string value = lines[i].substr(sep + 1); //after the separator
		while (!value.empty() && value[0] == ' ')
			value.erase(0, 1); //this erases 1 char startng by position 0
		request.headers[toLower(key)] = value; //add or update the key value
	}
}

// ===== Parse body cpp11 version=====
// void parseBody(HttpRequest& request, const std::string& bodyPart)
// {
// 	if (request.headers.count("Content-Length"))
// 	{
// 		int length = std::stoi(request.headers["Content-Length"]);
// 		if ((int)bodyPart.size() < length)
// 		{
// 			std::cout << "Body incomplete\n";
// 			return;
// 		}
// 		request.body = bodyPart.substr(0, length);
// 	}
// }

// ===== Parse body cpp98 version=====
bool HttpRequestParser::parseBody(HttpRequest& request, const std::string& bodyPart)
{
	std::map<std::string, std::string>::const_iterator it = request.headers.find("content-length");
	if (it == request.headers.end())
	{
		request.body = "";
		return true;
	}
	std::istringstream iss(it->second);
	int length;
	iss >> length;
	if (iss.fail() || !iss.eof() || length < 0)
		throw std::runtime_error("Invalid Content-Length");
	if ((int)bodyPart.size() < length)
		return false;
	request.body = bodyPart.substr(0, length);
	return true;
}
