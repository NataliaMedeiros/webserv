#include "HttpRequest.hpp"

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

	std::cout << "=== Testing incomplete request===\n";
	std::cout << findHeaderEnd(IncompleteRawRequest);
	std::cout << "=== Find headers end===\n";
	std::cout << "Headers end at position: " << findHeaderEnd(rawRequest);
	std::string headerPart, bodyPart;
	splitHeaderBody(rawRequest, headerPart, bodyPart);

    // std::cout << "\n=== Step 3: Split headers into lines ===\n";
    // std::vector<std::string> lines = splitLines(headerPart);

    // std::cout << "\n=== Step 4: Parse start line ===\n";
    // HttpRequest request;
    // parseStartLine(request, lines[0]);

    // std::cout << "\n=== Step 5: Parse headers ===\n";
    // parseHeaders(request, lines);

    // std::cout << "\n=== Step 6: Parse body ===\n";
    // parseBody(request, bodyPart);

    // std::cout << "\n=== Final HttpRequest object ===\n";
    // std::cout << "Method: " << request.method << "\n";
    // std::cout << "Path: " << request.path << "\n";
    // std::cout << "Version: " << request.version << "\n";
    // for (auto& h : request.headers)
    //     std::cout << h.first << ": " << h.second << "\n";
    // std::cout << "Body: " << request.body << "\n";

    return 0;
}
