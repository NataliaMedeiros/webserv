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
 * Decode percent-encoded sequences in a URI path.
 * Returns false for malformed encodings or embedded null bytes.
 */
static bool percentDecodePath(const std::string& input, std::string& output)
{
    output.clear();
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

        if (i + 2 >= input.size())
            return false;

        int high = hexValue(input[i + 1]);
        int low = hexValue(input[i + 2]);

        if (high < 0 || low < 0)
            return false;

        char decoded = static_cast<char>((high << 4) | low);
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
 * Check that candidatePath is equal to basePath or inside it.
 */
static bool pathBeginsWithPath(const std::string& base, const std::string& candidate)
{
    if (base == "/")
        return candidate.size() > 0 && candidate[0] == '/';

    if (candidate == base)
        return true;

    if (candidate.size() <= base.size())
        return false;

    if (candidate.compare(0, base.size(), base) != 0)
        return false;

    return candidate[base.size()] == '/';
}

/*
 * Ensure an existing requested path stays inside the configured location root.
 * Non-existing paths are allowed to continue so the normal flow can return 404.
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
 * Get a header value. Tries both lowercase and canonical casing.
 */
static bool getHeaderValue(const HttpRequest& req,
                           const std::string& lowercaseName,
                           const std::string& canonicalName,
                           std::string& value)
{
    std::map<std::string, std::string>::const_iterator it;

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

    for (std::vector<std::string>::const_iterator it = rd.methods.begin();
         it != rd.methods.end(); ++it)
    {
        if (*it == method)
            return true;
    }

    return false;
}

/*
 * Join allowed methods for the HTTP Allow header.
 */
std::string Handler::joinAllowedMethods(const RouteDecision& rd) const
{
    if (rd.methods.empty())
        return "GET, POST, DELETE";

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

    size_t boundaryPos = contentType.find("boundary=");
    if (boundaryPos == std::string::npos)
    {
        std::cerr << "Boundary not found in Content-Type header\n";
        return false;
    }

    std::string boundaryValue = contentType.substr(boundaryPos + 9);

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

    if (boundaryValue.size() >= 2
        && boundaryValue[0] == '"'
        && boundaryValue[boundaryValue.size() - 1] == '"')
    {
        boundaryValue = boundaryValue.substr(1, boundaryValue.size() - 2);
    }

    if (boundaryValue.empty())
        return false;

    std::string boundary = "--" + boundaryValue;

    size_t partStart = req.body.find(boundary);
    if (partStart == std::string::npos)
    {
        std::cerr << "Boundary not found in request body\n";
        return false;
    }

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
        root.erase(root.size() - 1);

    if (req.path == "/")
        return root + "/" + rd.index;

    std::string remainder = req.path;

    if (!rd.locationPath.empty() && rd.locationPath != "/")
    {
        if (remainder.compare(0, rd.locationPath.size(), rd.locationPath) == 0)
            remainder = remainder.substr(rd.locationPath.size());
    }

    if (remainder.empty() || remainder[0] != '/')
        remainder = "/" + remainder;

    return root + remainder;
}

/*
 * Handle CGI execution.
 */
HttpResponse Handler::handleCgi(const RouteDecision& rd,
                                const HttpRequest& req,
                                const std::string& fullPath)
{
    if (!FileSystem::exists(fullPath))
        return makeError(rd, 404, "Not Found");

    if (!FileSystem::isFileNormal(fullPath))
        return makeError(rd, 403, "Forbidden");

    int inPipe[2];
    int outPipe[2];

    if (pipe(inPipe) == -1)
        return makeError(rd, 500, "Could not create pipes");

    if (pipe(outPipe) == -1)
    {
        close(inPipe[0]);
        close(inPipe[1]);
        return makeError(rd, 500, "Could not create pipes");
    }

    pid_t pid = fork();
    if (pid == -1)
    {
        close(inPipe[0]);
        close(inPipe[1]);
        close(outPipe[0]);
        close(outPipe[1]);
        return makeError(rd, 500, "Fork failed");
    }

    if (pid == 0)
    {
        dup2(inPipe[0], STDIN_FILENO);
        dup2(outPipe[1], STDOUT_FILENO);

        close(inPipe[0]);
        close(inPipe[1]);
        close(outPipe[0]);
        close(outPipe[1]);

        std::string contentLength = numberToString(req.body.size());

        std::vector<std::string> envStrings;
        envStrings.push_back("REQUEST_METHOD=" + req.method);
        envStrings.push_back("CONTENT_LENGTH=" + contentLength);
        envStrings.push_back("SCRIPT_FILENAME=" + fullPath);
        envStrings.push_back("GATEWAY_INTERFACE=CGI/1.1");
        envStrings.push_back("SERVER_PROTOCOL=HTTP/1.1");
        envStrings.push_back("QUERY_STRING=");
        envStrings.push_back("PATH_INFO=" + req.path);

        std::vector<char*> envp;
        for (std::vector<std::string>::iterator it = envStrings.begin();
             it != envStrings.end(); ++it)
        {
            envp.push_back(const_cast<char*>(it->c_str()));
        }
        envp.push_back(NULL);

        char* argv[] = {
            const_cast<char*>(rd.cgiPass.c_str()),
            const_cast<char*>(fullPath.c_str()),
            NULL
        };

        execve(rd.cgiPass.c_str(), argv, &envp[0]);
        perror("execve failed");
        _exit(1);
    }

    close(inPipe[0]);
    close(outPipe[1]);

    if (!req.body.empty())
        write(inPipe[1], req.body.c_str(), req.body.size());

    close(inPipe[1]);

    std::string output;
    char buffer[4096];
    ssize_t n;

    while ((n = read(outPipe[0], buffer, sizeof(buffer))) > 0)
        output.append(buffer, n);

    close(outPipe[0]);

    int status = 0;
    waitpid(pid, &status, 0);

    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
    {
        std::cerr << "CGI exit status: " << status << std::endl;
        return makeError(rd, 502, "Bad Gateway");
    }
    HttpResponse res;
    res.status = 200;
    res.reason = HttpResponse::reasonPhrase(200);

    size_t sep = output.find("\r\n\r\n");
    size_t offset = 4;

    if (sep == std::string::npos)
    {
        sep = output.find("\n\n");
        offset = 2;
    }

    if (sep != std::string::npos)
        res.setBody(output.substr(sep + offset), "text/html; charset=utf-8");
    else
        res.setBody(output, "text/html; charset=utf-8");

    return res;
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

    html << "<!DOCTYPE html>\n"
         << "<html><head><title>Index of " << htmlEscape(displayPath) << "</title></head>\n"
         << "<body>\n"
         << "<h1>Index of " << htmlEscape(displayPath) << "</h1>\n"
         << "<hr>\n<ul>\n";

    if (displayPath != "/")
        html << "  <li><a href=\"../\">../</a></li>\n";

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

    if (hasParentDirectorySegment(req.path))
        return makeError(rd, 403, "Forbidden");

    std::string fullPath = buildPath(rd, req);

    if (!isPathInsideRoot(rd.root, fullPath))
        return makeError(rd, 403, "Forbidden");
    std::cout << "CGI DEBUG\n";
    std::cout << "cgiPass: [" << rd.cgiPass << "]\n";
    std::cout << "cgiExtension: [" << rd.cgiExtension << "]\n";
    std::cout << "path: [" << fullPath << "]\n";
    if (!rd.cgiPass.empty()
    && (rd.cgiExtension.empty() || hasExtension(fullPath, rd.cgiExtension)))
    {
        std::cout << "ENTERING CGI\n";
        return handleCgi(rd, req, fullPath);
    }

    if (!isMethodAllowed(rd, req.method))
    {
        HttpResponse res = makeError(rd, 405, "Method Not Allowed");
        res.setHeader("Allow", joinAllowedMethods(rd));
        return res;
    }

    if (req.method == "POST" && !rd.uploadPath.empty())
        return handleUpload(rd, req);

    if (FileSystem::isDir(fullPath))
    {
        if (req.method != "GET")
            return makeError(rd, 403, "Forbidden");

        std::string withIndex = fullPath;

        if (!withIndex.empty() && withIndex[withIndex.size() - 1] != '/')
            withIndex += '/';

        withIndex += rd.index;

        if (FileSystem::isFileNormal(withIndex))
            return handleStaticFile(rd, withIndex);

        if (rd.autoindex)
            return handleAutoindex(fullPath, req.path);

        return makeError(rd, 404, "Not Found");
        // return makeError(rd, 403, "Forbidden");
    }

    if (req.method == "GET")
        return handleStaticFile(rd, fullPath);

    if (req.method == "DELETE")
        return handleDelete(rd, fullPath);

    return makeError(rd, 501, "Not Implemented");
}
