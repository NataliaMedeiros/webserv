#include "Handler.hpp"
#include "FileSystem.hpp"
#include <cstdio>   // std::remove
#include <string>
#include <iostream>

// ─────────────────────────────────────────────
// Top-level dispatch
// ─────────────────────────────────────────────
//
// Order matters. Each step either returns a response or falls through.
//
//   1. Redirect configured?      → 301/302 response, done
//   2. Method allowed?           → 405 if not
//   3. Build the on-disk path
//   4. Path is a directory?      → try index file, else 403
//   5. Method-specific handling  → GET serves file, DELETE removes it
//   6. Nothing matched           → 405


// ─────────────────────────────────────────────
// Redirect (301/302/307/308)
// ─────────────────────────────────────────────
//
// res.headers is a std::map<std::string, std::string>.
// The [] operator receives a key (a string we pass in). It then looks
// inside the map for that key:
//   - if the key already exists → it returns a reference to its value
//   - if the key does NOT exist → it inserts a new entry with that key
//     and an empty-string value, then returns a reference to it
// Either way, we get a reference we can assign to with =.
////here,   key   = "Location"       (the HTTP header name)
//   value = rd.redirectUrl   (the URL the browser should go to)
// So `headers["Location"] = rd.redirectUrl;` means:
//   "make sure there's an entry with key 'Location', then set its value
//    to rd.redirectUrl."
HttpResponse Handler::handleRedirect(const RouteDecision& rd)
{
    HttpResponse res;
    res.status = rd.redirectCode;

    if      (rd.redirectCode == 301) res.reason = "Moved Permanently";
    else if (rd.redirectCode == 302) res.reason = "Found";
    else if (rd.redirectCode == 307) res.reason = "Temporary Redirect";
    else if (rd.redirectCode == 308) res.reason = "Permanent Redirect";
    else                             res.reason = "Redirect";

    res.headers["Location"]       = rd.redirectUrl;
    res.headers["Content-Length"] = "0";
    return res;
}

// ─────────────────────────────────────────────
// Method check
// ─────────────────────────────────────────────
//
// Empty methods list = no restriction = allow everything.
// Otherwise the request method must appear in the list of the RouteDecision's allowed methods
//which comes from the configuration file.
bool Handler::isMethodAllowed(const RouteDecision& rd, const std::string& method)
{
    if (rd.methods.empty())
        return true;
    for (const std::string& m : rd.methods) 
    {
        if (m == method)
            return true;
    }
    return false;
}

// ─────────────────────────────────────────────
// Error response with a tiny HTML body
// ─────────────────────────────────────────────
HttpResponse Handler::makeError(int code, const std::string& message)
{
    HttpResponse res;
    res.status = code;
    res.reason = message;

    std::string body = "<html><body><h1>"
                     + std::to_string(code) + " " + message
                     + "</h1></body></html>\n";
    res.body = body;
    res.headers["Content-Type"] = "text/html";
    return res;
}

// ─────────────────────────────────────────────
// Path building
// ─────────────────────────────────────────────
//
// Takes the matched location's root and the request URI,
// joins them into one filesystem path.
//
// Special case: request for "/" means "serve the index file at root".
// Resolves the request URI to a filesystem path using alias-style stripping.
//
// The subject says:
//   "if URL /kapouet is rooted to /tmp/www,
//    URL /kapouet/pouic/toto/pouet will search for /tmp/www/pouic/toto/pouet"
// That means the matched location prefix is REMOVED from the URI before
// it's appended to the root. Example:
//   locationPath = "/images"
//   root         = "./www/images"
//   req.path     = "/images/cat.jpg"
//   remainder    = "/cat.jpg"         (prefix stripped)
//   result       = "./www/images/cat.jpg"

std::string Handler::buildPath(const RouteDecision& rd, const HttpRequest& req)
{
    std::string root = rd.root;
    if (!root.empty() && root.back() == '/')//.back returns a reference to the last character of the string
        root.pop_back();//.pop_back() removes the last character from the string, here removes the trailing slash if there is one

    // Special case: request for "/" means serve the index file at root.
    if (req.path == "/")
        return root + "/" + rd.index;

    // Strip the matched location prefix from the URI.
    // If the location was "/", there's nothing to strip.
    std::string remainder = req.path;
    if (!rd.locationPath.empty() && rd.locationPath != "/")
    {
        if (remainder.compare(0, rd.locationPath.size(), rd.locationPath) == 0)//if the request path starts with the matched location path
            remainder = remainder.substr(rd.locationPath.size());
    }

    // Make sure remainder starts with a slash before joining.
    if (remainder.empty() || remainder[0] != '/')
        remainder = "/" + remainder;

    return root + remainder;
}

// ─────────────────────────────────────────────
// Static file (GET)
// ─────────────────────────────────────────────
HttpResponse Handler::handleStaticFile(const std::string& fullPath)
{
    if (!FileSystem::exists(fullPath))
        return makeError(404, "Not Found");

    std::string content;
    if (!FileSystem::readFile(fullPath, content))
        return makeError(500, "Could not read file");

    HttpResponse res;
    res.body = content;
    res.headers["Content-Type"] = FileSystem::mimeType(fullPath);
    return res;
}

// ─────────────────────────────────────────────
// DELETE
// ─────────────────────────────────────────────
//
// 204 No Content is the standard response for a successful delete
// when there is nothing useful to return in the body.
HttpResponse Handler::handleDelete(const std::string& fullPath)
{
    if (!FileSystem::exists(fullPath))
        return makeError(404, "Not Found");

    if (std::remove(fullPath.c_str()) != 0)
        return makeError(500, "Could not delete file");

    HttpResponse res;
    res.status = 204;
    res.reason = "No Content";
    return res;
}

HttpResponse Handler::handle(const RouteDecision& rd, const HttpRequest& req)
{
    
    //redirectCode == 0 means no redirect configured, so we only handle redirect if it's non-zero
    if (rd.redirectCode != 0)
        return handleRedirect(rd);
    if (!isMethodAllowed(rd, req.method))
        return makeError(405, "Method Not Allowed");
    std::string fullPath = buildPath(rd, req);
    if (FileSystem::isDir(fullPath))//if the path is a directory we serve the index file if it exists, otherwise return 403
    {
        std::string withIndex = fullPath;
        if (withIndex.back() != '/')//if the directory path doesn't already end with a slash
            withIndex += '/';//add a slash so we don't get double slashes
        withIndex += rd.index;//append the index filename to the directory path

        if (FileSystem::isFileNormal(withIndex))
            return handleStaticFile(withIndex);

        return makeError(403, "Forbidden");
    }
    if (req.method == "GET")
        return handleStaticFile(fullPath);
    if (req.method == "DELETE")
        return handleDelete(fullPath);
    // POST with upload, CGI, etc. — not implemented yet
    return makeError(405, "Method Not Allowed");
}

//BIIGER VERSION
// #include "Handler.hpp"
// #include "FileSystem.hpp"

// HttpResponse Handler::handleRedirect(const RouteDecision& rd)
// {
//     HttpResponse res;
//     res.status = rd.redirectCode;

//     // Assign reason text for the most common redirect codes
//     if (rd.redirectCode == 301)      res.reason = "Moved Permanently";
//     else if (rd.redirectCode == 302) res.reason = "Found";
//     else if (rd.redirectCode == 307) res.reason = "Temporary Redirect";
//     else if (rd.redirectCode == 308) res.reason = "Permanent Redirect";
//     else                             res.reason = "Redirect";

//     res.headers["Location"] = rd.redirectUrl;
//     res.headers["Content-Length"] = "0";
//     return res;
// }


// bool Handler::isMethodAllowed(const RouteDecision& rd, const std::string& method)
// {
//     // Empty methods list = no restriction configured = allow all
//     if (rd.methods.empty())
//         return true;

//     for (const std::string& m : rd.methods)
//     {
//         if (m == method)//if request method matches the routeDecision's allowed method
//             return true;
//     }
//     return false;
// }

// HttpResponse Handler::handle(const RouteDecision& rd, const HttpRequest& req)
// {
//     // 1. Redirect takes priority over everything else
//     // if (rd.redirectCode != 0)
//     //     return handleRedirect(rd);

//     // 2. Check method is allowed
//     if (!isMethodAllowed(rd, req.method))
//         return makeError(405, "Method Not Allowed");

//     // 3. Build the filesystem path for this request
//     std::string fullPath = buildPath(rd, req);

//     // 4. Path is a directory
//     if (FileSystem::isDir(fullPath))
//     {
//         // Try index file first
//         std::string withIndex = fullPath;
//         if (withIndex.back() != '/')
//             withIndex += '/';
//         withIndex += rd.index;

//         if (FileSystem::isFileNormal(withIndex))
//             return handleStaticFile(withIndex);

//         // No index file — autoindex?
//         if (rd.autoindex)
//             return handleAutoindex(fullPath, req.path);

//         return makeError(403, "Forbidden");
//     }
//     // 5. CGI
//     if (!rd.cgiPass.empty())
//         return handleCgi(rd, req, fullPath);

//     // 6. Upload (POST with upload path configured)
//     if (req.method == "POST" && !rd.uploadPath.empty())
//         return handleUpload(rd, req);

//     // 7. Static file (GET/DELETE or plain POST without upload config)
//     if (req.method == "GET")
//         return handleStaticFile(fullPath);

//     if (req.method == "DELETE")
//     {
//         if (!FileSystem::exists(fullPath))
//             return makeError(404, "Not Found");
//         // Actual deletion will use std::remove — fine for now
//         if (std::remove(fullPath.c_str()) != 0)
//             return makeError(500, "Could not delete file");
//         HttpResponse res;
//         res.status = 204;
//         res.reason = "No Content";
//         return res;
//     }
//     return makeError(405, "Method Not Allowed");
// }

// std::string Handler::buildPath(const RouteDecision& rd, const HttpRequest& req)
// {
//     std::string root = rd.root;

//     // Strip trailing slash from root so we don't get double slashes
//     if (!root.empty() && root.back() == '/')
//         root.pop_back();

//     if (req.path == "/")
//         return root + "/" + rd.index;

//     return root + req.path;
// }

// // ─────────────────────────────────────────────
// // PRIVATE helpers
// // ─────────────────────────────────────────────



// HttpResponse Handler::handleStaticFile(const std::string& fullPath)
// {
//     if (!FileSystem::exists(fullPath))
//         return makeError(404, "Not Found");

//     std::string content;
//     if (!FileSystem::readFile(fullPath, content))
//         return makeError(500, "Could not read file");

//     HttpResponse res;
//     res.setBody(content, FileSystem::mimeType(fullPath));
//     return res;
// }

// HttpResponse Handler::handleAutoindex(const std::string& dirPath,
//                                        const std::string& uriPath)
// {
//     std::vector<std::string> entries;
//     if (!FileSystem::listDir(dirPath, entries))
//         return makeError(500, "Could not read directory");

//     // Sort alphabetically so the listing is predictable
//     std::sort(entries.begin(), entries.end());

//     // Build a simple HTML page
//     std::ostringstream html;
//     html << "<!DOCTYPE html>\n"
//          << "<html>\n<head><title>Index of " << uriPath << "</title></head>\n"
//          << "<body>\n"
//          << "<h1>Index of " << uriPath << "</h1>\n"
//          << "<hr>\n<ul>\n";

//     // Link back to parent (unless already at root)
//     if (uriPath != "/")
//         html << "  <li><a href=\"../\">../</a></li>\n";

//     for (const std::string& name : entries)
//     {
//         std::string href = uriPath;
//         if (href.back() != '/')
//             href += '/';
//         href += name;
//         html << "  <li><a href=\"" << href << "\">" << name << "</a></li>\n";
//     }

//     html << "</ul>\n<hr>\n</body>\n</html>\n";

//     HttpResponse res;
//     res.setBody(html.str(), "text/html");
//     return res;
// }

// HttpResponse Handler::handleCgi(const RouteDecision& rd, const HttpRequest& req,
//                                   const std::string& fullPath)
// {
//     // CGI is the most complex feature — saved for last.
//     // This stub returns a clear message so you know the path reached here.
//     // Replace this entire function body when you implement CGI.
//     (void)rd;
//     (void)req;
//     (void)fullPath;

//     HttpResponse res;
//     res.status = 501;
//     res.reason = "Not Implemented";
//     res.setBody("CGI not implemented yet\n", "text/plain");
//     return res;
// }

// HttpResponse Handler::handleUpload(const RouteDecision& rd, const HttpRequest& req)
// {
//     if (rd.uploadPath.empty())
//         return makeError(400, "No upload path configured");

//     // Build destination: uploadPath + last segment of request path
//     std::string filename = req.path;
//     size_t slash = filename.rfind('/');
//     if (slash != std::string::npos)
//         filename = filename.substr(slash + 1);
//     if (filename.empty())
//         filename = "upload";

//     std::string dest = rd.uploadPath;
//     if (dest.back() != '/')
//         dest += '/';
//     dest += filename;

//     if (!FileSystem::writeFile(dest, req.body))
//         return makeError(500, "Could not write upload");

//     HttpResponse res;
//     res.status = 201;
//     res.reason = "Created";
//     res.headers["Location"] = "/" + filename;
//     res.headers["Content-Length"] = "0";
//     return res;
// }

// HttpResponse Handler::makeError(int code, const std::string& message)
// {
//     HttpResponse res;
//     res.status = code;
//     res.reason = message;

//     std::string body = "<html><body><h1>"
//                      + std::to_string(code) + " " + message
//                      + "</h1></body></html>\n";
//     res.setBody(body, "text/html");
//     return res;
// }
