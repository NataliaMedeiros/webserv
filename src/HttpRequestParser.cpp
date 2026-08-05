#include "HttpRequestParser.hpp"

#include <iostream>
#include <sstream>
#include <stdexcept>
#include <cctype>
#include <vector>
#include <cstdlib>
#include <algorithm> // std::min
#include <cerrno>    // errno, ERANGE

const size_t HttpRequestParser::MAX_HEADER_SIZE;

// HttpRequestParser::HttpRequestParser(const std::function < size_t(const std::string&) >& maxBodySizeFor)
//     : _maxBodySizeFor(maxBodySizeFor), maxBodySize(0) {}

HttpRequestParser::HttpRequestParser(const std::function<size_t(const std::string&)>& maxBodySizeFor)
    : _maxBodySizeFor(maxBodySizeFor), _state(ReadingHeaders), maxBodySize(0)
    , _contentLength(0), _chunkRemaining(0)
{
}

// void HttpRequestParser::setMaxBodySize(size_t size)
// {
//     maxBodySize = size;
// }

// const HttpRequest& HttpRequestParser::currentRequest() const
// {
//     return _currentRequest;
// }

static bool shouldKeepAlive(const HttpRequest& req);

static std::string percentDecode(const std::string& s);

HttpRequestParser::Result HttpRequestParser::feed(
    const std::string& chunk,
    HttpRequest& req
)
{
    _buf += chunk;

    try
    {
        while (true)
        {
            /*
             * Parse the request line and headers only once.
             */
            if (_state == ReadingHeaders)
            {
                const size_t headerEnd =
                    findHeaderEnd(_buf);

                if (headerEnd == std::string::npos)
                {
                    if (_buf.size() > MAX_HEADER_SIZE)
                    {
                        _buf.clear();
                        resetRequestState();

                        std::cerr
                            << "URI_TOO_LONG 1\n";

                        return UriTooLong;
                    }

                    return NeedMore;
                }

                // headerEnd points to the start of "\r\n\r\n".
                // Subtract 4 so the delimiter is included in the limit.
                if (headerEnd > MAX_HEADER_SIZE - 4)
                {
                    _buf.clear();
                    resetRequestState();

                    std::cerr
                        << "URI_TOO_LONG 1\n";

                    return UriTooLong;
                }

                /*
                 * Copy only the headers, then remove them from
                 * the buffer. From this point, _buf contains
                 * only body data or a pipelined next request.
                 */
                const std::string headerPart =
                    _buf.substr(0, headerEnd);

                _buf.erase(0, headerEnd + 4);

                const std::vector<std::string> lines =
                    splitLines(headerPart);

                if (lines.empty())
                {
                    throw std::runtime_error(
                        "empty request headers"
                    );
                }

                parseFirstLine(
                    _currentRequest,
                    lines[0]
                );

                if (!_maxBodySizeFor)
                {
                    throw std::runtime_error(
                        "missing max body size resolver"
                    );
                }

                maxBodySize =
                    _maxBodySizeFor(
                        _currentRequest.path
                    );

                parseHeaders(
                    _currentRequest,
                    lines
                );

                /*
                 * HTTP/1.1 requires the Host header.
                 */
                const std::map<
                    std::string,
                    std::string
                >::const_iterator hostIt =
                    _currentRequest.headers.find(
                        "host"
                    );

                if (
                    hostIt
                        == _currentRequest.headers.end()
                    || trim(hostIt->second).empty()
                )
                {
                    throw std::runtime_error(
                        "missing Host header"
                    );
                }

                const std::map<
                    std::string,
                    std::string
                >::const_iterator transferEncodingIt =
                    _currentRequest.headers.find(
                        "transfer-encoding"
                    );

                const std::map<
                    std::string,
                    std::string
                >::const_iterator contentLengthIt =
                    _currentRequest.headers.find(
                        "content-length"
                    );

                /*
                 * Having both headers makes the body boundary
                 * ambiguous and may enable request smuggling.
                 */
                if (
                    transferEncodingIt
                        != _currentRequest.headers.end()
                    && contentLengthIt
                        != _currentRequest.headers.end()
                )
                {
                    throw std::runtime_error(
                        "both Transfer-Encoding and "
                        "Content-Length are present"
                    );
                }

                if (
                    transferEncodingIt
                    != _currentRequest.headers.end()
                )
                {
                    if (
                        toLower(
                            trim(
                                transferEncodingIt->second
                            )
                        )
                        != "chunked"
                    )
                    {
                        throw std::runtime_error(
                            "unsupported Transfer-Encoding"
                        );
                    }

                    _state = ReadingChunkSize;
                    continue;
                }

                if (
                    contentLengthIt
                    != _currentRequest.headers.end()
                )
                {
                    _contentLength =
                        parseContentLength(
                            contentLengthIt->second
                        );

                    if (
                        static_cast<
                            unsigned long long
                        >(_contentLength)
                        >
                        static_cast<
                            unsigned long long
                        >(maxBodySize)
                    )
                    {
                        throw std::runtime_error(
                            "PAYLOAD_TOO_LARGE"
                        );
                    }

                    if (_contentLength == 0)
                        return finishRequest(req);

                    _state = ReadingFixedBody;
                    continue;
                }

                /*
                 * POST requests must tell the server where
                 * their body ends.
                 */
                if (_currentRequest.method == "POST")
                {
                    throw std::runtime_error(
                        "POST without Content-Length "
                        "or Transfer-Encoding"
                    );
                }

                return finishRequest(req);
            }

            if (_state == ReadingFixedBody)
                return parseFixedBody(req);

            /*
             * ReadingChunkSize, ReadingChunkData,
             * ReadingChunkDataCrlf and ReadingChunkTrailers
             * are all handled by the incremental chunk parser.
             */
            return parseChunkedBody(req);
        }
    }
    catch (const std::runtime_error& error)
    {
        const std::string message =
            error.what();

        std::cerr
            << "Exception: ["
            << message
            << "]\n";

        _buf.clear();
        resetRequestState();

        if (message == "PAYLOAD_TOO_LARGE")
            return PayloadTooLarge;

        std::cerr << "BADREQUEST 3\n";
        return BadRequest;
    }
}

std::string HttpRequestParser::toLower(std::string s)
{
	for (size_t i = 0; i < s.size(); i++)
		s[i] = std::tolower(s[i]);
	return s;
}

// Remove leading and trailing whitespace/tabs/CR from a string.
std::string HttpRequestParser::trim(const std::string& s)
{
	size_t start = 0;
	while (start < s.size() &&
	       (s[start] == ' ' || s[start] == '\t' || s[start] == '\r'))
		++start;

	size_t end = s.size();
	while (end > start &&
	       (s[end - 1] == ' ' || s[end - 1] == '\t' || s[end - 1] == '\r'))
		--end;

	return s.substr(start, end - start);
}


size_t HttpRequestParser::findHeaderEnd(const std::string& raw)
{
	return raw.find("\r\n\r\n");
}

std::vector<std::string> HttpRequestParser::splitLines(const std::string& headerPart)
{
	std::vector<std::string> lines; //Create a container to store each line separately
	size_t start = 0;

	while (true)
	{
		size_t end = headerPart.find("\r\n", start);
		if (end == std::string::npos)
		{ // it goes inside this just in case there's no more lines
			if (start < headerPart.size())
				lines.push_back(headerPart.substr(start));
			break;
		}
		lines.push_back(headerPart.substr(start, end - start));//Add line to the end of the vector lines
		start = end + 2; // skip "\r\n"
	}
	return lines;
}

void HttpRequestParser::parseFirstLine(HttpRequest& request, const std::string& startLine)
{
	size_t firstSpace = startLine.find(' ');
	if (firstSpace == std::string::npos)
		throw std::runtime_error("Invalid start line: missing first space in request line");
	size_t secondSpace = startLine.find(' ', firstSpace + 1);
	if (secondSpace == std::string::npos)
		throw std::runtime_error("Invalid start line: missing second space in request line");
	request.method = startLine.substr(0, firstSpace);
	request.path = startLine.substr(firstSpace + 1, secondSpace - firstSpace - 1);
	request.version = startLine.substr(secondSpace + 1);

	if (!isValidMethod(request.method))
		throw std::runtime_error("unsupported method: " + request.method);
	if (!isValidVersion(request.version))
		throw std::runtime_error("unsupported version: " + request.version);
	// Catches empty paths and absolute URIs like http://host/path
	// that browsers send when talking to a proxy.
	if (request.path.empty() || request.path[0] != '/')
		throw std::runtime_error("invalid path: " + request.path);
	splitPathQuery(request);
}

// ===== Parse headers =====
void HttpRequestParser::parseHeaders(HttpRequest& request, const std::vector<std::string>& lines)
{
	for (size_t i = 1; i < lines.size(); i++) //headers starts at line 1
	{
		size_t sep = lines[i].find(":"); //: is the separator
		if (sep == std::string::npos)
			throw std::runtime_error("malformed header line");

		// RFC 7230 3.2.4: no whitespace allowed between the header field name
		// and the colon. Silently trimming it away would hide a smuggling risk.
		std::string rawKey = lines[i].substr(0, sep);
		if (!rawKey.empty() && (rawKey[rawKey.size() - 1] == ' ' || rawKey[rawKey.size() - 1] == '\t'))
			throw std::runtime_error("whitespace before header colon");

		std::string key = toLower(trim(rawKey)); //before the separator
		std::string value = trim(lines[i].substr(sep + 1)); //after the separator

		if (key.empty())
			throw std::runtime_error("empty header name");

		if (key == "content-length" && request.headers.count(key) > 0)
			throw std::runtime_error("duplicate Content-Length header");
		request.headers[key] = value; //add or update the key value
	}
}

bool HttpRequestParser::isValidMethod(const std::string& method)
{
	return method == "GET" || method == "POST" || method == "DELETE" || method == "HEAD" ;
}

bool HttpRequestParser::isValidVersion(const std::string& version)
{
	return version == "HTTP/1.1";
}

void HttpRequestParser::splitPathQuery(HttpRequest& req)
{
	size_t qmark = req.path.find('?');
	if (qmark == std::string::npos)
		return; // no query string — nothing to split

	req.query_string = req.path.substr(qmark + 1);
	req.path         = req.path.substr(0, qmark);
}

// / Decode %XX percent-encoding back to the original byte.
// e.g. "%20" -> " ", "%2B" -> "+"
/* C++ language num formulário. O navegador monta essa URL:
GET /search?q=C%2B%2B+language HTTP/1.1
espaço  →  +
+       →  %2B
ã       →  %C3%A3
/       →  %2F   (quando / é dado e não separador de path)*/
static std::string percentDecode(const std::string& s)
{
	std::string out;
	out.reserve(s.size()); //memory allocation

	for (size_t i = 0; i < s.size(); i++)
	{
		if (s[i] == '+')
		{
			out += ' '; // HTML form encoding uses + for space
		}
		else if (s[i] == '%' && i + 2 < s.size() &&
		         std::isxdigit(static_cast<unsigned char>(s[i + 1])) &&
		         std::isxdigit(static_cast<unsigned char>(s[i + 2])))
		{
			// Convert two hex digits to a single byte.
			char hex[3] = { s[i + 1], s[i + 2], '\0' };
			out += static_cast<char>(std::strtol(hex, NULL, 16)); // convert from hexadecimal to ascii value
			i += 2; // skip the two hex digits we just consumed
		}
		else
		{
			out += s[i];
		}
	}
	return out;
}

// Parse "key=value&key2=value2" into req.query_params.
// Each key and value is percent-decoded.
void HttpRequestParser::parseQueryString(HttpRequest& req)
{
	if (req.query_string.empty())
		return;

	const std::string& qs = req.query_string;
	size_t start = 0;

	while (start <= qs.size())
	{
		// Find the end of this key=value pair.
		size_t amp = qs.find('&', start);
		size_t end = (amp == std::string::npos) ? qs.size() : amp;

		std::string pair = qs.substr(start, end - start);

		if (!pair.empty())
		{
			size_t eq = pair.find('=');
			std::string key, value;

			if (eq == std::string::npos)
			{
				// A key with no '=' — treat value as empty string.
				key = percentDecode(pair);
			}
			else
			{
				key   = percentDecode(pair.substr(0, eq));
				value = percentDecode(pair.substr(eq + 1));
			}

			if (!key.empty())
				req.query_params[key] = value;
		}

		if (amp == std::string::npos)
			break;
		start = amp + 1;
	}
}

// The browser by default keeps the connection OPEN, until it finds
static bool shouldKeepAlive(const HttpRequest& req)
{
    std::map<std::string, std::string>::const_iterator it =
        req.headers.find("connection");
	// in case the header is NOT find:
    if (it == req.headers.end()) // end means that element not find, so connection stays open
        return true; //in this case is true becase en http/1.1 if don't exist a header connection it automatically means the connection should keep-alive

    return HttpRequestParser::toLower(it->second) != "close";  // now it works because toLower is a public member now
}


/* Note for function ShouldKeepAlive:
Noor: So the ShouldKeepAlive functions searches in the header folder to the key "connection".
Headers are during parsing always saved as a key in small letters (with the toLower(trim()) function in parseHeaders).
So here in this function we searh for "conecction", this always works,
even if the browser "Connection:code" sends with Capitals in it) */
long HttpRequestParser::parseContentLength(
    const std::string& value
) const
{
    if (value.empty())
    {
        throw std::runtime_error(
            "empty Content-Length"
        );
    }

    for (size_t i = 0; i < value.size(); ++i)
    {
        if (!std::isdigit(
                static_cast<unsigned char>(value[i])
            ))
        {
            throw std::runtime_error(
                "non-digit in Content-Length"
            );
        }
    }

    errno = 0;
    char* endPointer = NULL;

    const long length = std::strtol(
        value.c_str(),
        &endPointer,
        10
    );

    if (
        errno == ERANGE
        || endPointer == value.c_str()
        || *endPointer != '\0'
        || length < 0
    )
    {
        throw std::runtime_error(
            "invalid Content-Length value"
        );
    }

    return length;
}

long HttpRequestParser::parseChunkSize(
    const std::string& value
) const
{
    if (value.empty())
    {
        throw std::runtime_error(
            "empty chunk size line"
        );
    }

    for (size_t i = 0; i < value.size(); ++i)
    {
        if (!std::isxdigit(
                static_cast<unsigned char>(value[i])
            ))
        {
            throw std::runtime_error(
                "non-hex character in chunk size"
            );
        }
    }

    errno = 0;
    char* endPointer = NULL;

    const long chunkSize = std::strtol(
        value.c_str(),
        &endPointer,
        16
    );

    if (
        errno == ERANGE
        || endPointer == value.c_str()
        || *endPointer != '\0'
        || chunkSize < 0
    )
    {
        throw std::runtime_error(
            "invalid chunk size"
        );
    }

    return chunkSize;
}

HttpRequestParser::Result
HttpRequestParser::parseFixedBody(
    HttpRequest& req
)
{
    const size_t expectedSize =
        static_cast<size_t>(_contentLength);

    const size_t alreadyRead =
        _currentRequest.body.size();

    if (alreadyRead > expectedSize)
    {
        throw std::runtime_error(
            "body exceeds Content-Length"
        );
    }

    const size_t remaining =
        expectedSize - alreadyRead;

    const size_t bytesToConsume =
        std::min(remaining, _buf.size());

    if (bytesToConsume > 0)
    {
        _currentRequest.body.append(
            _buf,
            0,
            bytesToConsume
        );

        _buf.erase(
            0,
            bytesToConsume
        );
    }

    if (
        _currentRequest.body.size()
        < expectedSize
    )
    {
        return NeedMore;
    }

    return finishRequest(req);
}

HttpRequestParser::Result
HttpRequestParser::parseChunkedBody(
    HttpRequest& req
)
{
    while (true)
    {
        /*
         * Waiting for:
         *
         * <hex-size>\r\n
         */
        if (_state == ReadingChunkSize)
        {
            const size_t crlf =
                _buf.find("\r\n");

            if (crlf == std::string::npos)
            {
                /*
                 * Prevent an endless chunk-size line from
                 * growing the buffer indefinitely.
                 */
                if (_buf.size() > MAX_HEADER_SIZE)
                {
                    throw std::runtime_error(
                        "chunk size line too long"
                    );
                }

                return NeedMore;
            }

            std::string sizeLine =
                _buf.substr(0, crlf);

            _buf.erase(
                0,
                crlf + 2
            );

            /*
             * Ignore optional chunk extensions:
             *
             * 1000;name=value\r\n
             */
            const size_t semicolon =
                sizeLine.find(';');

            if (semicolon != std::string::npos)
            {
                sizeLine =
                    sizeLine.substr(
                        0,
                        semicolon
                    );
            }

            sizeLine = trim(sizeLine);

            const long chunkSize =
                parseChunkSize(sizeLine);

            /*
             * The zero chunk marks the end of the body.
             * We still need to consume the final CRLF or
             * optional trailer headers.
             */
            if (chunkSize == 0)
            {
                _state = ReadingChunkTrailers;
                continue;
            }

            /*
             * Keep chunkSize as long during parsing.
             * Compare before converting it to size_t.
             */
            if (
                _currentRequest.body.size()
                    > maxBodySize
                || static_cast<unsigned long long>(
                        chunkSize
                    )
                    > static_cast<unsigned long long>(
                        maxBodySize
                        - _currentRequest.body.size()
                    )
            )
            {
                throw std::runtime_error(
                    "PAYLOAD_TOO_LARGE"
                );
            }

            _chunkRemaining =
                static_cast<size_t>(
                    chunkSize
                );

            _state = ReadingChunkData;
            continue;
        }

        /*
         * Consume only the currently available bytes
         * belonging to this chunk.
         */
        if (_state == ReadingChunkData)
        {
            if (_buf.empty())
                return NeedMore;

            const size_t bytesToConsume =
                std::min(
                    _chunkRemaining,
                    _buf.size()
                );

            _currentRequest.body.append(
                _buf,
                0,
                bytesToConsume
            );

            _buf.erase(
                0,
                bytesToConsume
            );

            _chunkRemaining -=
                bytesToConsume;

            if (_chunkRemaining != 0)
                return NeedMore;

            _state = ReadingChunkDataCrlf;
            continue;
        }

        /*
         * Every chunk data section must end with CRLF.
         */
        if (_state == ReadingChunkDataCrlf)
        {
            if (_buf.size() < 2)
                return NeedMore;

            if (
                _buf[0] != '\r'
                || _buf[1] != '\n'
            )
            {
                throw std::runtime_error(
                    "missing CRLF after chunk data"
                );
            }

            _buf.erase(0, 2);

            _state = ReadingChunkSize;
            continue;
        }

        /*
         * After the zero chunk, the usual ending is:
         *
         * 0\r\n
         * \r\n
         *
         * Trailer headers may also appear before the
         * final empty line.
         */
        if (_state == ReadingChunkTrailers)
        {
            if (_buf.size() < 2)
                return NeedMore;

            /*
             * No trailer headers:
             *
             * the buffer starts directly with "\r\n".
             */
            if (
                _buf[0] == '\r'
                && _buf[1] == '\n'
            )
            {
                _buf.erase(0, 2);
                return finishRequest(req);
            }

            /*
             * Trailer headers exist. Wait for their
             * terminating empty line.
             */
            const size_t trailerEnd =
                _buf.find("\r\n\r\n");

            if (trailerEnd == std::string::npos)
            {
                if (_buf.size() > MAX_HEADER_SIZE)
                {
                    throw std::runtime_error(
                        "chunk trailers too large"
                    );
                }

                return NeedMore;
            }

            _buf.erase(
                0,
                trailerEnd + 4
            );

            return finishRequest(req);
        }

        throw std::runtime_error(
            "invalid chunk parser state"
        );
    }
}

HttpRequestParser::Result
HttpRequestParser::finishRequest(
    HttpRequest& req
)
{
    parseQueryString(
        _currentRequest
    );

    _currentRequest.keepAlive =
        shouldKeepAlive(
            _currentRequest
        );

    /*
     * Copy the completed request to the caller before
     * resetting the internal parser state.
     */
    req = _currentRequest;

    resetRequestState();

    return Complete;
}

void HttpRequestParser::resetRequestState()
{
    _currentRequest = HttpRequest();

    _state = ReadingHeaders;

    maxBodySize = 0;
    _contentLength = 0;
    _chunkRemaining = 0;
}
