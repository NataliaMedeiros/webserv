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
		// bool parse(const std::string& raw, HttpRequest& request);
		enum Result
		{
			NeedMore,
			Complete,
			BadRequest,
			PayloadTooLarge,
			UriTooLong
		};

		// Protects against clients that send endless headers (slow loris).
		// HttpRequestParser();
		explicit HttpRequestParser(
			const std::function<
				size_t(const std::string&)
			>& maxBodySizeFor
		);
		// void setMaxBodySize(size_t size);
		// const HttpRequest& currentRequest() const;
		// static const size_t MAX_BUFFER_SIZE = 8 * 1024 * 1024; // 8 MB
		static const size_t MAX_HEADER_SIZE = 64 * 1024;
		static std::string toLower(std::string s); // Noor: moved it to public! and made static
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
		// Current parsing phase.
		HttpRequest _currentRequest;
		// Number of bytes still missing from the current chunk.
		// HttpRequest _currentRequest;
		std::function<size_t(const std::string&)> _maxBodySizeFor;
    	ParseState _state;
		size_t maxBodySize;
		long _contentLength;
		size_t _chunkRemaining;
		static std::string trim(const std::string& s);
		size_t findHeaderEnd(const std::string& raw);
		// void splitHeaderBody(const std::string& raw, std::string& headerPart, std::string& bodyPart);
		std::vector<std::string> splitLines(const std::string& headerPart);
		void parseFirstLine(HttpRequest& request, const std::string& startLine);
		void parseHeaders(HttpRequest& request, const std::vector<std::string>& lines);
		// bool parseBody(HttpRequest& req, const std::string& bodyPart, size_t& bodyBytesConsumed);
		static bool isValidMethod(const std::string& method);
		static bool isValidVersion(const std::string& version);
		void splitPathQuery(HttpRequest& req);
		static void parseQueryString(HttpRequest& req);
		// static bool parseChunkedBody(HttpRequest& req, const std::string& bodyPart, size_t& bytesConsumed);
		// bool parseChunkedBody(HttpRequest& req, const std::string& bodyPart, size_t& bytesConsumed);
		long parseContentLength(const std::string& value) const; //Interpreta e valida o header Content-Length.
		long parseChunkSize(const std::string& value) const;//Interpreta o tamanho hexadecimal de um chunk.
		Result parseFixedBody(HttpRequest& req);//Consome incrementalmente um body com Content-Length.
		Result parseChunkedBody(HttpRequest& req);//Consome incrementalmente um body chunked.
		Result finishRequest(HttpRequest& req);//Finaliza e entrega a request ao ClientConnection.
		void resetRequestState();//Limpa o estado interno para a próxima request.
		// Content-Length of the current request.
};

#endif

