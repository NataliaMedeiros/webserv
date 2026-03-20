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
	std::cout << "Headers end at position: " << pos << "\n";
	//pos finds the position where "\r\n\r\n" starts, what means the end of the request
	return pos;
}

// void splitHeaderBody(const std::string& raw, std::string& headerPart, std::string& bodyPart) {
//     size_t pos = findHeaderEnd(raw);
//     if (pos == std::string::npos) return;

//     headerPart = raw.substr(0, pos);
//     bodyPart = raw.substr(pos + 4);
//     std::cout << "Header part:\n" << headerPart << "\n";
//     std::cout << "Body part:\n" << bodyPart << "\n";
// }

// // ===== Step 3: Split headers into lines =====
// std::vector<std::string> splitLines(const std::string& headerPart) {
//     std::vector<std::string> lines;
//     std::istringstream stream(headerPart);
//     std::string line;
//     while (std::getline(stream, line)) {
//         if (!line.empty() && line.back() == '\r')
//             line.pop_back();
//         lines.push_back(line);
//     }

//     std::cout << "Header lines:\n";
//     for (const auto& l : lines) std::cout << l << "\n";
//     return lines;
// }

// // ===== Step 4: Parse start line =====
// void parseStartLine(HttpRequest& request, const std::string& startLine) {
//     std::istringstream ss(startLine);
//     ss >> request.method >> request.path >> request.version;
//     std::cout << "Parsed start line:\n";
//     std::cout << "Method: " << request.method << "\n";
//     std::cout << "Path: " << request.path << "\n";
//     std::cout << "Version: " << request.version << "\n";
// }

// // ===== Step 5: Parse headers =====
// void parseHeaders(HttpRequest& request, const std::vector<std::string>& lines) {
//     for (size_t i = 1; i < lines.size(); i++) {
//         size_t sep = lines[i].find(":");
//         if (sep == std::string::npos) continue;

//         std::string key = lines[i].substr(0, sep);
//         std::string value = lines[i].substr(sep + 1);
//         if (!value.empty() && value[0] == ' ')
//             value.erase(0, 1);
//         request.headers[key] = value;
//     }

//     std::cout << "Parsed headers:\n";
//     for (const auto& h : request.headers)
//         std::cout << h.first << ": " << h.second << "\n";
// }

// // ===== Step 6: Parse body =====
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
