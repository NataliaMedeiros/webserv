// This HttpRequest turns that messy string into clean, usable data.

#ifndef	HTTPREQUEST_HPP
#define HTTPREQUEST_HPP

#include <string>
#include <map>
#include <iostream>
#include <sstream>
#include <vector>
#include <stdexcept>

class HttpRequest
{
	public:
		std::string method; //Take the first word from the request
		std::string path;
		std::string version;
		std::map<std::string,std::string> headers;
		std::string body;
};

size_t findHeaderEnd(const std::string& raw)
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

void splitHeaderBody(const std::string& raw, std::string& headerPart, std::string& bodyPart)
{
	size_t pos = findHeaderEnd(raw);

	if (pos == std::string::npos)
		return;
	headerPart = raw.substr(0, pos);
	bodyPart = raw.substr(pos + 4);
	std::cout << "Header part:\n" << headerPart << "\n";
	std::cout << "Body part:\n" << bodyPart << "\n";
}

std::vector<std::string> splitLines(const std::string& headerPart)
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
	for (const auto& l : lines) std::cout << l << "\n";
	return lines;
}

//I think the manual is better
std::vector<std::string> splitLinesManual(const std::string& headerPart)
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
	std::cout << "Header lines:\n";
	for (const auto& l : lines) std::cout << l << "\n";
	return lines;
}

void parseFirstLine(HttpRequest& request, const std::string& startLine)
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
void parseHeaders(HttpRequest& request, const std::vector<std::string>& lines)
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
		request.headers[key] = value; //add or update the key value
	}
}

// // ===== Parse body =====
// void parseBody(HttpRequest& request, const std::string& bodyPart) {
//     if (request.headers.count("Content-Length")) {
//         int length = std::stoi(request.headers["Content-Length"]);
//         if ((int)bodyPart.size() < length) {
//             std::cout << "Body incomplete\n";
//             return;
//         }
//         request.body = bodyPart.substr(0, length);
//     }
//     std::cout << "Parsed body:\n" << request.body << "\n";
// }

#endif

/*
Giving this request example:
	POST /submit HTTP/1.1\r\n
	Host: example.com\r\n
	Content-Length: 11\r\n
	\r\n
	Hello World

The method takes the first word from the request:
	POST

The path takes the URL path the client wants:
	/login

The version takes the HTTP version used by the client:
	HTTP/1.1

The headers takes the Key-value storage of all headers:
	headers["Host"] = "example.com";
	headers["Content-Length"] = "11";
**Content-Length = exact number of bytes in the body
Is good to use maps for the headers because:
	We can easily search:
		request.headers["Content-Length"]
The body takes everything AFTER \r\n\r\n:
	body = Hello World;
*/

/* **IMPORTANT**
	. Headers are case-insensitive
	. Everything is stored as string
	. This class does NOT parse anything
	*/
