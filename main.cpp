#include "HttpRequest.hpp"
#include "HttpRequestParser.hpp"

#include "HttpRequestParser.hpp"
#include "HttpRequest.hpp"
#include <iostream>

int main()
{
    HttpRequestParser parser;
    HttpRequest req;

    HttpRequestParser::Result res;

    res = parser.feed("POST /login HTTP/1.1\r\nHost: example.com\r\nCont", req);
    std::cout << "res 1 = " << res << "\n";

    res = parser.feed("ent-Length: 12\r\n\r\nuser", req);
    std::cout << "res 2 = " << res << "\n";

    res = parser.feed("name=joe", req);
    std::cout << "res 3 = " << res << "\n";

    if (res == HttpRequestParser::Complete)
    {
        std::cout << "Method: " << req.method << "\n";
        std::cout << "Path: " << req.path << "\n";
        std::cout << "Version: " << req.version << "\n";

        std::cout << "Headers:\n";
        for (std::map<std::string, std::string>::const_iterator it = req.headers.begin();
             it != req.headers.end(); ++it)
            std::cout << it->first << ": " << it->second << "\n";

        std::cout << "Body: " << req.body << "\n";
    }

	///////
	std::cout << "Test 1: complete basic request \n";
	HttpRequest req1;
	HttpRequestParser parser1;
	HttpRequestParser::Result res1 =
		parser1.feed("GET / HTTP/1.1\r\nHost: example.com\r\n\r\n", req1);
	std::cout << "res = " << res1 << "\n";
	std::cout << "method = " << req1.method << "\n";
	std::cout << "path = " << req1.path << "\n";
	std::cout << "version = " << req1.version << "\n";
	std::cout << "body = [" << req1.body << "]\n";
	std::cout << "\n";

	///////
	std::cout << "test 2: Post splited in chunks \n";
	HttpRequest req2;
	HttpRequestParser parser2;
	HttpRequestParser::Result res2;
	res2 = parser2.feed("POST /login HTTP/1.1\r\nHost: example.com\r\nCont", req2);
	std::cout << "res1 = " << res2 << "\n";
	res2 = parser2.feed("ent-Length: 12\r\n\r\nuser", req2);
	std::cout << "res2 = " << res2 << "\n";
	res2 = parser2.feed("name=joe", req2);
	std::cout << "res3 = " << res2 << "\n";
	if (res2 == HttpRequestParser::Complete)
		std::cout << "body = " << req2.body << "\n";
	std::cout << "\n";


	/////
	std::cout << "Teste 3 — Content-Length em maiúsculas/minúsculas misturadas\n";
	HttpRequest req3;
	HttpRequestParser parser3;
	HttpRequestParser::Result res3 =
		parser3.feed("POST /login HTTP/1.1\r\nHost: example.com\r\nCoNtEnT-LeNgTh: 12\r\n\r\nusername=joe", req3);
	std::cout << "res = " << res3 << "\n";
	std::cout << "body = " << req3.body << "\n";
	std::cout << "\n";


	//////
	std::cout << "Teste 4 — Request inválida\n";
	HttpRequest req4;
	HttpRequestParser parser4;
	HttpRequestParser::Result res4 =
		parser4.feed("GET\r\n\r\n", req4);
	std::cout << "res = " << res4 << "\n";
	std::cout << "\n";

	//////
	std::cout << "Teste 5 — Body incompleto\n";
	HttpRequest req5;
	HttpRequestParser parser5;

	HttpRequestParser::Result res5 =
		parser5.feed("POST /upload HTTP/1.1\r\nHost: example.com\r\nContent-Length: 10\r\n\r\nhel", req5);

	std::cout << "res = " << res5 << "\n";

	return (1);
}


// int main()
// {
// 	std::string IncompleteRawRequest =
// 		"POST /login HTTP/1.1\r\n"
// 		"Host: example.com\r\n"
// 		"Content-Length: 12\r\n"
// 		""
// 		"username=joe";

//     std::string rawRequest =
//         "POST /login HTTP/1.1\r\n"
//         "Host: example.com\r\n"
//         "Content-Length: 12\r\n"
//         "\r\n"
//         "username=joe";

//     HttpRequest request;
// 	HttpRequestParser parser;

//     try
// 	{
// 		bool complete = parser.parse(rawRequest, request);

// 		if (!complete)
// 		{
// 			std::cout << "Request incomplete\n";
// 			return 0;
// 		}

// 		std::cout << "Method: " << request.method << "\n";
// 		std::cout << "Path: " << request.path << "\n";
// 		std::cout << "Version: " << request.version << "\n";

// 		std::cout << "\nHeaders:\n";
// 		for (std::map<std::string, std::string>::const_iterator it = request.headers.begin();
// 			it != request.headers.end(); ++it)
// 		{
// 			std::cout << it->first << ": " << it->second << "\n";
// 		}

// 		std::cout << "\nBody:\n" << request.body << "\n";
// 	}
// 	catch (const std::exception& e)
// 	{
// 		std::cout << "Parse error: " << e.what() << "\n";
// 	}

// 	return 0;
// }

	// std::cout << "=== Testing incomplete request===\n";
	// std::cout << findHeaderEnd(IncompleteRawRequest);
	// std::cout << "=== Find headers end===\n";
	// std::cout << "Headers end at position: " << findHeaderEnd(rawRequest);
	// std::string headerPart, bodyPart;
	// splitHeaderBody(rawRequest, headerPart, bodyPart);

	// std::cout << "\n=== Split headers into lines manually ===\n";
	// std::vector<std::string> linesManual = splitLinesManual(headerPart);

    // std::cout << "\n=== Parse first line ===\n";
    // HttpRequest request;
    // parseFirstLine(request, linesManual[0]);
	// std::cout << "Method: " << request.method << "\n";
	// std::cout << "Path: " << request.path << "\n";
	// std::cout << "Version: " << request.version << "\n";

    // std::cout << "\n=== Parse headers ===\n";
    // parseHeaders(request, linesManual);
    // // for (const auto& h : request.headers) // auto allowed cpp11
    // //     std::cout << h.first << ": " << h.second << "\n";
    // for (std::vector<std::string>::const_iterator it = linesManual.begin();
	// 		it != linesManual.end(); ++it)
	// {
	// 	 std::cout << *it << "\n";
	// }


    // std::cout << "\n=== Parse body ===\n";
    // parseBody(request, bodyPart);
    // std::cout << "Parsed body:\n" << request.body << "\n";

    // std::cout << "\n=== Final HttpRequest object ===\n";
    // std::cout << "Method: " << request.method << "\n";
    // std::cout << "Path: " << request.path << "\n";
    // std::cout << "Version: " << request.version << "\n";
    // for (auto& h : request.headers)
    //     std::cout << h.first << ": " << h.second << "\n";
    // std::cout << "Body: " << request.body << "\n";
