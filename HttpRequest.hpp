// This HttpRequest turns that messy string into clean, usable data.

#ifndef	HTTPREQUEST.HPP
#define HTTPREQUEST.HPP

#include<string>
#include <map>

class HttpRequest
{
	public:
		std::string method; //Take the first word from the request
		std::string path;
		std::string version;
		std::map<std::string,std::string> headers;
		std::string body;
};

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
