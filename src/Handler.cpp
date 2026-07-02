#include "Handler.hpp"
#include "FileSystem.hpp"

#include <algorithm>
#include <cstdio>
#include <sstream>
#include <vector>
#include <stdlib.h>
#include <limits.h>

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
            case '"': escaped += "&quot;"; break;
            default:   escaped += input[i];  break;
        }
    }
    return escaped;
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
    res.reason = message.empty() ? HttpResponse::reasonPhrase(code) : message;

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
         << "<body><h1>" << code << ' ' << res.reason << "</h1></body></html>\n";
    res.setBody(body.str(), "text/html; charset=utf-8");
    return res;
}

/*
 * Convert the request URI into a filesystem path for the matched location.
 * The matched location prefix is removed before appending the path to root.
 */
std::string Handler::buildPath(const RouteDecision& rd, const HttpRequest& req)
{
    std::string root = rd.root;

    if (!root.empty() && root.back() == '/')
        root.pop_back();

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
 * Serve a regular static file.
 */
HttpResponse Handler::handleStaticFile(const RouteDecision& rd, const std::string& fullPath)
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
HttpResponse Handler::handleAutoindex(const std::string& dirPath, const std::string& uriPath)
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

// ─────────────────────────────────────────────
// DELETE
// ─────────────────────────────────────────────
HttpResponse Handler::handleDelete(const RouteDecision& rd, const std::string& fullPath)
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
    return res;
}

/*
 * Dispatch the request after routing: redirect, method check, security check,
 * directory handling, static GET, DELETE, or 501 for allowed unimplemented methods.
 */
HttpResponse Handler::handle(const RouteDecision& rd, const HttpRequest& req)
{
    if (rd.redirectCode != 0)
        return handleRedirect(rd);

    if (!isMethodAllowed(rd, req.method))
    {
        HttpResponse res = makeError(rd, 405, "Method Not Allowed");
        res.setHeader("Allow", joinAllowedMethods(rd));
        return res;
    }

    if (hasParentDirectorySegment(req.path))
        return makeError(rd, 403, "Forbidden");

    std::string fullPath = buildPath(rd, req);

    if (!isPathInsideRoot(rd.root, fullPath))
        return makeError(rd, 403, "Forbidden");

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

        return makeError(rd, 403, "Forbidden");
    }

    if (req.method == "GET")
        return handleStaticFile(rd, fullPath);

    if (req.method == "DELETE")
        return handleDelete(rd, fullPath);

    return makeError(rd, 501, "Not Implemented");
}
