#ifndef HTTPREQUESTPARSER_HPP
#define HTTPREQUESTPARSER_HPP

#include "HttpRequest.hpp"
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <cctype>
#include <vector>
#include <functional>
#include <cstddef>
#include <string>

class HttpRequestParser
{
	public:
		enum Result
		{
			NeedMore,
			Complete,
			BadRequest,
			PayloadTooLarge,
			UriTooLong
		};

		// Protects against clients that send endless headers (slow loris).
		explicit HttpRequestParser(
			const std::function<
				size_t(const std::string&)
			>& maxBodySizeFor
		);
		static const size_t MAX_HEADER_SIZE = 64 * 1024;
		static std::string toLower(std::string s);
		Result feed(const std::string& chunk, HttpRequest& req);

	private:

		enum ParseState
		{
			ReadingHeaders,
			ReadingFixedBody,
			ReadingChunkSize,
			ReadingChunkData,
			ReadingChunkDataCrlf,
			ReadingChunkTrailers
		};
		std::string _buf;
		HttpRequest _currentRequest;
		std::function<size_t(const std::string&)> _maxBodySizeFor;
    	ParseState _state;
		size_t maxBodySize;
		long _contentLength;
		size_t _chunkRemaining;
		static std::string trim(const std::string& s);
		size_t findHeaderEnd(const std::string& raw);
		std::vector<std::string> splitLines(const std::string& headerPart);
		void parseFirstLine(HttpRequest& request, const std::string& startLine);
		void parseHeaders(HttpRequest& request, const std::vector<std::string>& lines);
		static bool isValidMethod(const std::string& method);
		static bool isValidVersion(const std::string& version);
		void splitPathQuery(HttpRequest& req);
		static void parseQueryString(HttpRequest& req);
		long parseContentLength(const std::string& value) const; // Parses and validates the Content-Length header.
		long parseChunkSize(const std::string& value) const; // Parses the hexadecimal size of a chunk.
		Result parseFixedBody(HttpRequest& req); // Incrementally consumes a body with Content-Length.
		Result parseChunkedBody(HttpRequest& req); // Incrementally consumes a chunked body.
		Result finishRequest(HttpRequest& req); // Finalizes and hands off the request to ClientConnection.
		void resetRequestState(); // Clears internal state for the next request.
};

#endif

