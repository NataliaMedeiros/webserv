#include "Handler.hpp"
#include "FileSystem.hpp"

#include <algorithm>
#include <cstdio>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include <stdlib.h>
#include <limits.h>
#include <unistd.h>
#include <sys/wait.h>
#include <cctype>

/*
 * Convert a number to string without std::to_string, to stay compatible with
 * older C++ standards if your Makefile uses -std=c++98.
 */
static std::string numberToString(size_t value)
{
    std::ostringstream out;
    out << value;
    return out.str();
}

/*
 * Decode hexadecimal characters used in percent-encoded URI paths.
 */
static int hexValue(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

/*
 * Decode %XX characters in the URI.
 * Example: "%20" becomes a space.
 * Return false if the encoding is invalid or contains '\0'.
 */
static bool percentDecodePath(const std::string& input, std::string& output)
{
    output.clear();//first clear the output string to ensure it's empty before decoding
    output.reserve(input.size());

    for (size_t i = 0; i < input.size(); ++i)
    {
        if (input[i] != '%')
        {
            if (input[i] == '\0')
                return false;
            output += input[i];
            continue;
        }

        if (i + 2 >= input.size())//not enough characters after '%' to form a valid hex code
            return false;

        int high = hexValue(input[i + 1]);//convert the first hex digit after '%' to its integer value
        int low = hexValue(input[i + 2]);

        if (high < 0 || low < 0)
            return false;

        char decoded = static_cast<char>((high << 4) | low);//combine the two hex digits into a single byte
        if (decoded == '\0')
            return false;

        output += decoded;
        i += 2;
    }

    return true;
}

/*
 * Detect path traversal attempts after percent-decoding the URI path.
 */
static bool hasParentDirectorySegment(const std::string& uriPath)
{
    std::string decoded;

    if (!percentDecodePath(uriPath, decoded))
        return true;

    size_t start = 0;
    while (start <= decoded.size())
    {
        size_t segmentEnd = start;

        while (segmentEnd < decoded.size()
                && decoded[segmentEnd] != '/'
                && decoded[segmentEnd] != '\\')
        {
            ++segmentEnd;
        }

        if (decoded.substr(start, segmentEnd - start) == "..")
            return true;

        if (segmentEnd == decoded.size())
            break;

        start = segmentEnd + 1;
    }

    return false;
}

/*
 * Resolve an existing path to its canonical filesystem path.
 */
static bool getRealPath(const std::string& path, std::string& out)
{
    char resolved[PATH_MAX];

    if (realpath(path.c_str(), resolved) == NULL)
        return false;

    out = resolved;
    return true;
}

/*
 * Check if candidate is the same as base or is inside base.
 * Example:
 *   base      = /var/www
 *   candidate = /var/www/images/cat.jpg  → true
 *   candidate = /var/www2/file.txt       → false
 */
static bool pathBeginsWithPath(const std::string& base, const std::string& candidate)
{
    // Special case: base is root, everything is inside it
    if (base == "/")
        return candidate.size() > 0 && candidate[0] == '/';

    // candidate is exactly the same as base
    if (candidate == base)
        return true;
    // Candidate must be longer to be inside base.
    if (candidate.size() <= base.size())
        return false;
    // Candidate must start with the base path.
    if (candidate.compare(0, base.size(), base) != 0)
        return false;
    // Make sure the match ends at a directory boundary.
    // Prevents "/var/www2" from matching "/var/www".
    return candidate[base.size()] == '/';
}

/*
 * Check that the requested path is inside the configured root.
 * If the path does not exist yet, allow it so it can return 404 later.
 */
static bool isPathInsideRoot(const std::string& root, const std::string& candidate)
{
    std::string realRoot;
    std::string realCandidate;

    if (!getRealPath(root, realRoot))
        return false;

    if (!getRealPath(candidate, realCandidate))
        return true;

    return pathBeginsWithPath(realRoot, realCandidate);
}
// Check if the path has the specified extension.

bool Handler::hasExtension(const std::string& path, const std::string& ext)
{
    if (ext.empty() || path.size() < ext.size())
        return false;
    return path.compare(path.size() - ext.size(), ext.size(), ext) == 0;
}

/*
 * Escape special HTML characters used in generated pages.
 */
static std::string htmlEscape(const std::string& input)
{
    std::string escaped;
    escaped.reserve(input.size());

    for (size_t i = 0; i < input.size(); ++i)
    {
        switch (input[i])
        {
            case '&':  escaped += "&amp;";  break;
            case '<':  escaped += "&lt;";   break;
            case '>':  escaped += "&gt;";   break;
            case '"':  escaped += "&quot;"; break;
            default:   escaped += input[i]; break;
        }
    }

    return escaped;
}

/*
 * Find a header in the request and store its value in `value`.
 * Returns true if the header was found, false otherwise.
 */
static bool getHeaderValue(const HttpRequest& req,
                           const std::string& lowercaseName,
                           const std::string& canonicalName,
                           std::string& value)
{
    std::map<std::string, std::string>::const_iterator it;
    //use iterator to access the headers map and find the header with the given name
    it = req.headers.find(lowercaseName);
    if (it != req.headers.end())
    {
        value = it->second;
        return true;
    }

    it = req.headers.find(canonicalName);
    if (it != req.headers.end())
    {
        value = it->second;
        return true;
    }

    return false;
}

/*
 * Build an HTTP redirect response.
 */
HttpResponse Handler::handleRedirect(const RouteDecision& rd)
{
    HttpResponse response;

    response.status = rd.redirectCode;
    response.reason = HttpResponse::reasonPhrase(rd.redirectCode);
    response.setHeader("Location", rd.redirectUrl);
    response.setHeader("Content-Length", "0");

    return response;
}

/*
 * Check whether the request method is allowed for the matched route.
 */
bool Handler::isMethodAllowed(const RouteDecision& rd, const std::string& method)
{
    if (rd.methods.empty())
        return true;
    //iterate over the allowed methods in the RouteDecision and check if the requested method is present
    for (std::vector<std::string>::const_iterator it = rd.methods.begin();
         it != rd.methods.end(); ++it)
    {
        if (*it == method)
            return true;
    }

    return false;
}

/*
 * Convert the allowed HTTP methods into a comma-separated string
 * for the HTTP Allow header.
 */
std::string Handler::joinAllowedMethods(const RouteDecision& rd) const
{
    if (rd.methods.empty())
        return "GET, POST, DELETE";//use default methods if none are specified

    std::ostringstream out;

    for (size_t i = 0; i < rd.methods.size(); ++i)
    {
        if (i != 0)
            out << ", ";
        out << rd.methods[i];
    }

    return out.str();
}

/*
 * Build an error response, using a configured error_page when available.
 */
HttpResponse Handler::makeError(const RouteDecision& rd, int code, const std::string& message)
{
    HttpResponse res;

    res.status = code;
    res.reason = HttpResponse::reasonPhrase(code);

    std::map<int, std::string>::const_iterator custom = rd.errorPages.find(code);
    if (custom != rd.errorPages.end())
    {
        std::string content;

        if (FileSystem::isFileNormal(custom->second)
            && FileSystem::readFile(custom->second, content))
        {
            res.setBody(content, FileSystem::mimeType(custom->second));
            return res;
        }
    }

    std::ostringstream body;
    body << "<!DOCTYPE html>\n"
         << "<html><head><title>" << code << ' ' << res.reason << "</title></head>\n"
         << "<body><h1>" << code << ' ' << res.reason << "</h1>\n";

    if (!message.empty() && message != res.reason)
        body << "<p>" << htmlEscape(message) << "</p>\n";

    body << "</body></html>\n";

    res.setBody(body.str(), "text/html; charset=utf-8");
    return res;
}

/*
 * Parse multipart/form-data request body to extract uploaded filename and content.
 */
bool Handler::parseMultipart(const HttpRequest& req,
                             std::string& outFilename,
                             std::string& outFileContent)
{
    std::string contentType;

    if (!getHeaderValue(req, "content-type", "Content-Type", contentType))
    {
        std::cerr << "Content-Type header not found\n";
        return false;
    }
    //boundary= is the marker used to separate parts inside a multipart request
    size_t boundaryPos = contentType.find("boundary=");
    if (boundaryPos == std::string::npos)
    {
        std::cerr << "Boundary not found in Content-Type header\n";
        return false;
    }
    //9 is the length of "boundary=" string, so we add it to get the start of the actual boundary value
    std::string boundaryValue = contentType.substr(boundaryPos + 9);
    // Remove any trailing semicolon and whitespace from the boundary value
    size_t semicolon = boundaryValue.find(';');
    if (semicolon != std::string::npos)
        boundaryValue = boundaryValue.substr(0, semicolon);

    while (!boundaryValue.empty()
           && (boundaryValue[boundaryValue.size() - 1] == '\r'
               || boundaryValue[boundaryValue.size() - 1] == '\n'
               || boundaryValue[boundaryValue.size() - 1] == ' '))
    {
        boundaryValue.erase(boundaryValue.size() - 1);
    }
    //boundary value may be quoted, so we remove the quotes if present
    if (boundaryValue.size() >= 2
        && boundaryValue[0] == '"'
        && boundaryValue[boundaryValue.size() - 1] == '"')
    {
        boundaryValue = boundaryValue.substr(1, boundaryValue.size() - 2);
    }

    if (boundaryValue.empty())
        return false;
    // In the body, the boundary is prefixed with "--".
    std::string boundary = "--" + boundaryValue;

    size_t partStart = req.body.find(boundary);
    if (partStart == std::string::npos)
    {
        std::cerr << "Boundary not found in request body\n";
        return false;
    }
    //Move past the boundary to get to the start of the part content
    partStart += boundary.size();

    size_t partEnd = req.body.find(boundary, partStart);
    if (partEnd == std::string::npos)
    {
        std::cerr << "Closing boundary not found in request body\n";
        return false;
    }

    std::string part = req.body.substr(partStart, partEnd - partStart);

    size_t fnPos = part.find("filename=\"");
    if (fnPos == std::string::npos)
    {
        std::cerr << "Filename not found in Content-Disposition header\n";
        return false;
    }

    fnPos += 10;

    size_t fnEnd = part.find("\"", fnPos);
    if (fnEnd == std::string::npos)
    {
        std::cerr << "Filename closing quote not found\n";
        return false;
    }

    outFilename = part.substr(fnPos, fnEnd - fnPos);
    // A blank line separates the multipart headers from the file content.
    size_t bodyStart = part.find("\r\n\r\n");
    if (bodyStart == std::string::npos)
    {
        std::cerr << "Body start not found after headers\n";
        return false;
    }

    bodyStart += 4;

    size_t bodyEnd = part.size();
    if (bodyEnd >= 2 && part[bodyEnd - 1] == '\n' && part[bodyEnd - 2] == '\r')
        bodyEnd -= 2;

    if (bodyEnd < bodyStart)
        return false;

    outFileContent = part.substr(bodyStart, bodyEnd - bodyStart);
    return true;
}

/*
 * Handle multipart upload.
 */
HttpResponse Handler::handleUpload(const RouteDecision& rd, const HttpRequest& req)
{
    if (rd.uploadPath.empty())
        return makeError(rd, 400, "No upload path configured");

    std::string filename;
    std::string content;

    if (!parseMultipart(req, filename, content))
        return makeError(rd, 400, "Malformed multipart body");

    if (filename.empty())
        return makeError(rd, 400, "No filename provided");
    // Prevent the client from using the filename to escape the upload directory.
    if (filename.find("..") != std::string::npos
        || filename.find('/') != std::string::npos
        || filename.find('\\') != std::string::npos)
    {
        return makeError(rd, 400, "Invalid filename");
    }

    std::string dest = rd.uploadPath;
    if (!dest.empty() && dest[dest.size() - 1] != '/')
        dest += '/';

    dest += filename;

    if (!FileSystem::writeFile(dest, content))
        return makeError(rd, 500, "Could not write upload");

    HttpResponse res;
    res.status = 201;
    res.reason = "Created";
    res.setHeader("Location", "/" + filename);
    res.setHeader("Content-Length", "0");

    return res;
}

/*
 * Convert the request URI into a filesystem path for the matched location.
 * The matched location prefix is removed before appending the path to root.
 */
std::string Handler::buildPath(const RouteDecision& rd, const HttpRequest& req)
{
    std::string root = rd.root;

    if (!root.empty() && root[root.size() - 1] == '/')
        root.erase(root.size() - 1);//remove trailing slash from root

    std::string remainder = req.path;

    if (!rd.locationPath.empty() && rd.locationPath != "/")
    {
        if (remainder.compare(0, rd.locationPath.size(), rd.locationPath) == 0)//check if the request path starts with the location path
            remainder = remainder.substr(rd.locationPath.size());//remove the matched location prefix from the request path
    }

    if (remainder.empty() || remainder[0] != '/')
        remainder = "/" + remainder;

    return root + remainder;
}

/*
* Build CGI environment variables. Used by ClientConnection's
* non-blocking startCgi().
*/
std::vector<std::string> Handler::buildCgiEnv(const RouteDecision& rd,
                                            const HttpRequest& req,
                                            const std::string& fullPath)
{
    (void)rd; // currently unused, kept for future headers like SCRIPT_NAME

    std::string contentLength = numberToString(req.body.size());

    std::vector<std::string> envStrings;
    envStrings.push_back("REQUEST_METHOD=" + req.method);
    envStrings.push_back("CONTENT_LENGTH=" + contentLength); // FIXED (16 july, by Noor): was CONTENT_LENTGH
    envStrings.push_back("SCRIPT_FILENAME=" + fullPath);
    envStrings.push_back("GATEWAY_INTERFACE=CGI/1.1");
    envStrings.push_back("SERVER_PROTOCOL=HTTP/1.1");
    envStrings.push_back("QUERY_STRING=" + req.query_string); // FIXED (Noor): was hardcoded empty, req.query_string is already parsed and available
    envStrings.push_back("PATH_INFO=" + req.path);

    // NEW (16 july, by Noor): expose every request header to the CGI script
    // as HTTP_<NAME>, per the CGI spec. e.g. "X-Secret: hello" becomes
    // HTTP_X_SECRET=hello. This is what the "special headers" test checks.
    for (std::map<std::string, std::string>::const_iterator it = req.headers.begin();
         it != req.headers.end(); ++it)
    {
        std::string envName = "HTTP_";

        for (size_t i = 0; i < it->first.size(); ++i)
        {
            char c = it->first[i];
            if (c == '-')
                envName += '_';
            else
                envName += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        }

        envStrings.push_back(envName + "=" + it->second);
    }

    return envStrings;
}

/*
 * Serve a regular static file.
 */
HttpResponse Handler::handleStaticFile(const RouteDecision& rd,
                                       const std::string& fullPath)
{
    if (!FileSystem::exists(fullPath))
        return makeError(rd, 404, "Not Found");

    if (!FileSystem::isFileNormal(fullPath))
        return makeError(rd, 403, "Forbidden");

    std::string content;
    if (!FileSystem::readFile(fullPath, content))
        return makeError(rd, 500, "Could not read file");

    HttpResponse res;
    res.setBody(content, FileSystem::mimeType(fullPath));

    return res;
}

/*
 * Generate a directory listing when autoindex is enabled.
 */
HttpResponse Handler::handleAutoindex(const std::string& dirPath,
                                      const std::string& uriPath)
{
    std::vector<std::string> entries;

    if (!FileSystem::listDir(dirPath, entries))
        return HttpResponse::error(500, "Could not read directory");

    std::sort(entries.begin(), entries.end());

    std::string displayPath = uriPath.empty() ? "/" : uriPath;
    std::ostringstream html;

    html << "<!doctype html>\n"
         << "<html lang=\"en\">\n"
         << "<head>\n"
         << "  <meta charset=\"utf-8\">\n"
         << "  <meta name=\"viewport\" "
         << "content=\"width=device-width, initial-scale=1\">\n"
         << "  <title>Index of "
         << htmlEscape(displayPath)
         << "</title>\n"
         << "  <link rel=\"stylesheet\" "
         << "href=\"/css/autoindex.css\">\n"
         << "</head>\n"
         << "<body>\n"
         << "  <main class=\"autoindex-card\">\n"
         << "    <header class=\"autoindex-header\">\n"
         << "      <div>\n"
         << "        <p class=\"eyebrow\">WEBSERV DIRECTORY</p>\n"
         << "        <h1>Index of "
         << htmlEscape(displayPath)
         << "</h1>\n"
         << "        <p class=\"subtitle\">"
         << "Browse the files available in this directory."
         << "</p>\n"
         << "      </div>\n"
         << "      <a class=\"home-button\" href=\"/\">Home</a>\n"
         << "    </header>\n"
         << "    <ul class=\"file-list\">\n";

    // Generate a list item for each entry in the directory
    for (std::vector<std::string>::const_iterator it = entries.begin();
         it != entries.end(); ++it)
    {
        std::string href = displayPath;

        if (href.empty() || href[href.size() - 1] != '/')
            href += '/';

        href += *it;

        html << "  <li><a href=\"" << htmlEscape(href) << "\">"
             << htmlEscape(*it) << "</a></li>\n";
    }

    html << "</ul>\n<hr>\n</body></html>\n";

    return HttpResponse::html(200, html.str());
}

/*
 * DELETE
 */
HttpResponse Handler::handleDelete(const RouteDecision& rd,
                                   const std::string& fullPath)
{
    if (!FileSystem::exists(fullPath))
        return makeError(rd, 404, "Not Found");

    if (!FileSystem::isFileNormal(fullPath))
        return makeError(rd, 403, "Forbidden");

    if (std::remove(fullPath.c_str()) != 0)
        return makeError(rd, 500, "Could not delete file");

    HttpResponse res;
    res.status = 204;
    res.reason = "No Content";
    res.setHeader("Content-Length", "0");

    return res;
}

/*
 * Dispatch the request after routing.
 */
HttpResponse Handler::handle(const RouteDecision& rd, const HttpRequest& req)
{

    if (rd.redirectCode != 0)
        return handleRedirect(rd);
    //security check: reject any request with ".." segments in the path
    if (hasParentDirectorySegment(req.path))
        return makeError(rd, 403, "Forbidden");

    std::string fullPath = buildPath(rd, req);

    if (!isPathInsideRoot(rd.root, fullPath))
        return makeError(rd, 403, "Forbidden");
    if (!isMethodAllowed(rd, req.method))
    {
        HttpResponse res = makeError(rd, 405, "Method Not Allowed");
        res.setHeader("Allow", joinAllowedMethods(rd));//make a comma-separated list of allowed methods and set it in the Allow header
        return res;
    }

    if (req.method == "POST" && !rd.uploadPath.empty())
        return handleUpload(rd, req);

    if (req.method == "POST" && rd.cgiPass.empty() && rd.uploadPath.empty())
    {
        HttpResponse res;
        res.status = 200;
        res.reason = "OK";
        res.setHeader("Content-Length", "0");
        return res;
    }

    if (FileSystem::isDir(fullPath))
    {
        if (req.method != "GET")
            return makeError(rd, 403, "Forbidden");

        std::string withIndex = fullPath;

        if (!withIndex.empty() && withIndex[withIndex.size() - 1] != '/')
            withIndex += '/';//append a trailing slash to the directory path if it doesn't already have one

        withIndex += rd.index;//append the index filename to the directory path

        if (FileSystem::isFileNormal(withIndex))
            return handleStaticFile(rd, withIndex);

        if (rd.autoindex)
            return handleAutoindex(fullPath, req.path);

        return makeError(rd, 404, "Not Found");
    }

    if (req.method == "GET")
        return handleStaticFile(rd, fullPath);

    if (req.method == "DELETE")
        return handleDelete(rd, fullPath);

    return makeError(rd, 501, "Not Implemented");
}
