#include "HttpRequestParser.hpp"

#include <iostream>
#include <sstream>
#include <stdexcept>
#include <cctype>

bool HttpRequestParser::parse(const std::string& raw, HttpRequest& request)
{
	std::string headerPart;
	std::string bodyPart;
	std::vector<std::string> lines;

	// 1. Check if headers are complete
	size_t pos = findHeaderEnd(raw);
	if (pos == std::string::npos)
		return false; // still waiting for more data

	// 2. Split headers and body
	splitHeaderBody(raw, headerPart, bodyPart);

	// 3. Split headers into lines
	lines = splitLines(headerPart);

	if (lines.empty())
		throw std::runtime_error("Empty request");

	// 4. Parse request line
	parseFirstLine(request, lines[0]);

	// 5. Parse headers
	parseHeaders(request, lines);

	// 6. Parse body
	if (!parseBody(request, bodyPart))
		return false; // body not complete yet

	return true; // request fully parsed
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
	// std::cout << "Headers end at position: " << pos << "\n";
	//pos finds the position where "\r\n\r\n" starts, what means the end of the request
	return pos;
}

void HttpRequestParser::splitHeaderBody(const std::string& raw, std::string& headerPart, std::string& bodyPart)
{
	size_t pos = findHeaderEnd(raw);

	if (pos == std::string::npos)
		return;
	headerPart = raw.substr(0, pos);
	bodyPart = raw.substr(pos + 4);
	std::cout << "Header part:\n" << headerPart << "\n";
	std::cout << "Body part:\n" << bodyPart << "\n";
}

std::vector<std::string> HttpRequestParser::splitLines(const std::string& headerPart)
{
	std::vector<std::string> lines; //Create a container to store each line separately
	std::istringstream stream(headerPart); //reads the input from  headerPart the same way std::cin does from terminal
	std::string line;

	while (std::getline(stream, line))
	{
		if (!line.empty() && line.back() == '\r')//back checks the last character
			line.pop_back();//pop_back removes the last character
		lines.push_back(line);
	}
	std::cout << "Header lines:\n";
	return lines;
}

//I think the manual is better
std::vector<std::string> HttpRequestParser::splitLinesManual(const std::string& headerPart)
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
