#include "HttpRequest.hpp"
#include "HttpRequestParser.hpp"

int main()
{
	std::string IncompleteRawRequest =
		"POST /login HTTP/1.1\r\n"
		"Host: example.com\r\n"
		"Content-Length: 12\r\n"
		""
		"username=joe";

    std::string rawRequest =
        "POST /login HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "Content-Length: 12\r\n"
        "\r\n"
        "username=joe";

    HttpRequest request;
	HttpRequestParser parser;

    try
	{
		bool complete = parser.parse(rawRequest, request);

		if (!complete)
		{
			std::cout << "Request incomplete\n";
			return 0;
		}

		std::cout << "Method: " << request.method << "\n";
		std::cout << "Path: " << request.path << "\n";
		std::cout << "Version: " << request.version << "\n";

		std::cout << "\nHeaders:\n";
		for (std::map<std::string, std::string>::const_iterator it = request.headers.begin();
			it != request.headers.end(); ++it)
		{
			std::cout << it->first << ": " << it->second << "\n";
		}

		std::cout << "\nBody:\n" << request.body << "\n";
	}
	catch (const std::exception& e)
	{
		std::cout << "Parse error: " << e.what() << "\n";
	}

	return 0;
}

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
