#include "HttpRequestParser.hpp"

#include <iostream>
#include <sstream>
#include <stdexcept>
#include <cctype>
#include <vector>
#include <cstdlib>

HttpRequestParser::HttpRequestParser()
    : maxBodySize(MAX_BUFFER_SIZE)
{
}

void HttpRequestParser::setMaxBodySize(size_t size)
{
    maxBodySize = size;
}

const HttpRequest& HttpRequestParser::currentRequest() const
{
    return _currentRequest;
}

static bool shouldKeepAlive(const HttpRequest& req);

static std::string percentDecode(const std::string& s);


HttpRequestParser::Result HttpRequestParser::feed(const std::string& chunk, HttpRequest& req)
{
    _buf += chunk;

	// Guard 1: buffer size limit.
	// A client sending endless headers without \r\n\r\n would grow _buf
	// forever. Reject early once we exceed the hard ceiling.
	if (_buf.size() > MAX_BUFFER_SIZE)
	{
		// This check runs before findHeaderEnd(), so we are always still
		// reading the request line or headers here, never the body.
		// That means the URI or headers are too long, not the payload.
		_buf.clear();
		std::cerr << "URI_TOO_LONG 1\n";
		return UriTooLong;
	}

	// We need the full header section before we can do anything.
	if (findHeaderEnd(_buf) == std::string::npos)
		return NeedMore;

	// Work on a fresh request object so that partial data from a
	// previous NeedMore call never leaks into the final result.
	HttpRequest tmp;
	std::string headerPart, bodyPart;

	splitHeaderBody(_buf, headerPart, bodyPart);

	std::vector<std::string> lines = splitLines(headerPart);

	if (lines.empty())
	{
		_buf.clear();
		std::cerr << "BADREQUEST 2\n";
		return BadRequest;
	}

	size_t bodyBytesConsumed = 0;

	try
	{
		parseFirstLine(tmp, lines[0]);
		parseHeaders(tmp, lines);
		// REDUNDANT!! 14 juli, Noor deleted this part:
		// std::map<std::string, std::string>::iterator cl =
    	// tmp.headers.find("content-length");

		// if (cl != tmp.headers.end())
		// {
    	// 	long length = std::atol(cl->second.c_str());
    	// 	if (length > static_cast<long>(maxBodySize))
    	// 	{
       	// 		 _buf.clear();
        // 		throw std::runtime_error("PAYLOAD_TOO_LARGE");
    	// 	}
		// }
		if (!parseBody(tmp, bodyPart, bodyBytesConsumed))
			return NeedMore;
	}
	catch (const std::runtime_error& e)
	{
    	std::cerr << "Exception: [" << e.what() << "]\n";

		_buf.clear();

    	if (std::string(e.what()) == "PAYLOAD_TOO_LARGE")
        	return PayloadTooLarge;

    	std::cerr << "BADREQUEST 3\n";
		return BadRequest;
	}
	std::map<std::string, std::string>::const_iterator hostIt = tmp.headers.find("host");
	if (hostIt == tmp.headers.end() || trim(hostIt->second).empty())
	{
		_buf.clear();
		std::cerr << "BADREQUEST 4\n";
		return BadRequest;
	}

	tmp.keepAlive = shouldKeepAlive(tmp);
	// Only write to the caller's req when the whole request is ready.
	req = tmp;
	// For now, clear everything after one request
	// Later you can consume only the used bytes
	_buf = _buf.substr(headerPart.size() + 4 + bodyBytesConsumed);
	return Complete;
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

void HttpRequestParser::splitHeaderBody(const std::string& raw, std::string& headerPart, std::string& bodyPart)
{
	size_t pos = findHeaderEnd(raw);

	if (pos == std::string::npos)
		return;
	headerPart = raw.substr(0, pos);
	bodyPart = raw.substr(pos + 4);
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
// //Entender melhor e checar se é a melhor maneira
// void HttpRequestParser::parseHeaders(HttpRequest& request, const std::vector<std::string>& lines)
// {
// 	for (size_t i = 1; i < lines.size(); i++) //headers starts at line 1
// 	{
// 		size_t sep = lines[i].find(":"); //: is the separator
// 		if (sep == std::string::npos)
// 			throw std::runtime_error("malformed header line");
// 		//	continue; //it will skip the line

// 		std::string key = toLower(trim(lines[i].substr(0, sep))); //before the separator
// 		std::string value = trim(lines[i].substr(sep + 1)); //after the separator
// 		// Two different values would let an attacker confuse the server about
// 		// where one request ends and the next begins.
// 		if (key.empty())
// 			continue;
// 		if (key == "content-length" && request.headers.count(key) > 0)
// 			throw std::runtime_error("duplicate Content-Length header");
// 		request.headers[key] = value; //add or update the key value
// 	}
// }
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

bool HttpRequestParser::parseBody(HttpRequest& req,
                                  const std::string& bodyPart,
                                  size_t& bodyBytesConsumed)
{
    std::map<std::string, std::string>::const_iterator teIt =
        req.headers.find("transfer-encoding");

    if (teIt != req.headers.end() && toLower(teIt->second) == "chunked")
    {
        size_t chunkedConsumed = 0;
        if (!parseChunkedBody(req, bodyPart, chunkedConsumed))
            return false;
        bodyBytesConsumed = chunkedConsumed;
        parseQueryString(req);
        return true;
    }

    std::map<std::string, std::string>::const_iterator clIt =
        req.headers.find("content-length");

    if (clIt != req.headers.end())
    {
        const std::string& clStr = clIt->second;
        for (size_t i = 0; i < clStr.size(); i++)
            if (!std::isdigit(static_cast<unsigned char>(clStr[i])))
                throw std::runtime_error("non-digit in Content-Length");

        std::istringstream iss(clStr);
        long length;
        iss >> length;
        if (iss.fail() || length < 0)
            throw std::runtime_error("invalid Content-Length value");
		if (length > static_cast<long>(maxBodySize))
    		throw std::runtime_error("PAYLOAD_TOO_LARGE");
        if (static_cast<long>(bodyPart.size()) < length)
            return false;
        req.body = bodyPart.substr(0, static_cast<size_t>(length));
        bodyBytesConsumed = static_cast<size_t>(length); // só os bytes do body
        parseQueryString(req);
        return true;
    }

    if (req.method == "POST")
        throw std::runtime_error("POST without Content-Length or Transfer-Encoding");

    req.body          = "";
    bodyBytesConsumed = 0;
    parseQueryString(req);
    return true;
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

// ============================================================
//  Body parsing
// ============================================================

// Un-chunk a chunked body.
//
// Chunked format (each chunk):
//   <hex-size>\r\n
//   <data of that many bytes>\r\n
// Terminated by:
//   0\r\n
//   \r\n
//
// Returns false if more data is needed.
// Throws on malformed chunk encoding.
bool HttpRequestParser::parseChunkedBody(HttpRequest& req,
										const std::string& bodyPart,
										size_t& bytesConsumed)
{
    std::string decoded;
    size_t pos = 0;

    while (pos < bodyPart.size())
    {
        size_t crlf = bodyPart.find("\r\n", pos);
        if (crlf == std::string::npos)
            return false;

        std::string sizeLine = bodyPart.substr(pos, crlf - pos);
        size_t semi = sizeLine.find(';');
        if (semi != std::string::npos)
            sizeLine = sizeLine.substr(0, semi);

        sizeLine = trim(sizeLine);
        if (sizeLine.empty())
            throw std::runtime_error("empty chunk size line");

        for (size_t i = 0; i < sizeLine.size(); i++)
            if (!std::isxdigit(static_cast<unsigned char>(sizeLine[i])))
                throw std::runtime_error("non-hex character in chunk size");

        long chunkSize = std::strtol(sizeLine.c_str(), NULL, 16);
        if (chunkSize < 0)
            throw std::runtime_error("negative chunk size");

        pos = crlf + 2;

        if (chunkSize == 0)
        {
            if (pos + 2 > bodyPart.size())
                return false;
            req.body     = decoded;
            bytesConsumed = pos + 2; // aponta para depois do \r\n final
            return true;
        }

        size_t dataEnd = pos + static_cast<size_t>(chunkSize);
        if (dataEnd + 2 > bodyPart.size())
            return false;

        if (bodyPart[dataEnd] != '\r' || bodyPart[dataEnd + 1] != '\n')
            throw std::runtime_error("missing CRLF after chunk data");

        //decoded += bodyPart.substr(pos, static_cast<size_t>(chunkSize));
		if (decoded.size() + static_cast<size_t>(chunkSize) > maxBodySize)
    throw std::runtime_error("PAYLOAD_TOO_LARGE");

decoded += bodyPart.substr(pos, static_cast<size_t>(chunkSize));
        pos = dataEnd + 2;
    }

    return false;
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
