#include "Handler.hpp"
#include "FileSystem.hpp"
#include <cstdio>   // std::remove
#include <string>
#include <iostream>
#include <algorithm>
#include <unistd.h>
#include <sys/wait.h>
#include <cstring>

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
// Upload (multipart/form-data POST)
// ─────────────────────────────────────────────

//Parses a multipart/form-data request body to extract the uploaded file's name and content

//In order, parser should do exactly this ---
// Read the boundary from the Content-Type header.
// Find the first --boundary.
// Read part headers until \r\n\r\n.
// Everything after that is file data.
// Stop at the next \r\n--boundary.
// If that boundary ends with --, you're done.

//Details:
//The function looks for the "boundary" string in the Content-Type header, 
//then finds the part of the body that contains the file data. 
//It extracts the filename from the Content-Disposition header and the file content from the body,
//returning them via outFilename and outFileContent. If any step fails (e.g., missing headers, malformed body), it returns false.
//example of a multipart/form-data request body:
//--boundary
//Content-Disposition: form-data; name="file"; filename="example.txt"
//Content-Type: text/plain
//This is the content of the file.
//--boundary--
//
//return example:
//outFilename = "example.txt"
//outFileContent = "This is the content of the file."



//auto deduces the type of the variable from the return type of req.headers.find(), which is 
//std::unordered_map<std::string, std::string>::iterator here
//find() searches the container for the key----(content-type) 
//If it finds the key ---- it returns an iterator to that element. 
//If it doesn’t find it --- it returns end().


bool Handler::parseMultipart(const HttpRequest& req, std::string& outFilename, std::string& outFileContent)
{
    auto it = req.headers.find("Content-Type");
    if (it == req.headers.end())
        return false;

    std::string contentType = it->second;//get the value of the content-type header
    size_t boundaryPos = contentType.find("boundary=");//find the position of the boundary parameter in the content-type header
    if (boundaryPos == std::string::npos)//if the boundary parameter is not found in the content-type header
        return false;

    std::string boundary = "--" + contentType.substr(boundaryPos + 9);//extract the boundary string from the content-type header, adding the leading "--" as per the multipart/form-data specification
    boundary = boundary.substr(0, boundary.find("\r")); 
    // Find the beginning and end of the part containing the file data
    size_t partStart = req.body.find(boundary);
    if (partStart == std::string::npos)//if the boundary is not found in the request body
        return false;
    partStart += boundary.size();//move past the boundary to the start of the part

    size_t partEnd = req.body.find(boundary, partStart);//find the next boundary after the start of the part
    if (partEnd == std::string::npos)
        return false;

    std::string part = req.body.substr(partStart, partEnd - partStart);//extract the part of the request body between the two boundaries

    // Extract filename from Content-Disposition header
    size_t fnPos = part.find("filename=\"");
    if (fnPos == std::string::npos)
        return false;
    fnPos += 10;//move past the "filename=\"" to the start of the filename
    size_t fnEnd = part.find("\"", fnPos);//find the end of the filename by looking for the next quote after the start of the filename
    outFilename = part.substr(fnPos, fnEnd - fnPos);//extract the filename from the part

    // Body starts after the blank line (\r\n\r\n) following the headers
    size_t bodyStart = part.find("\r\n\r\n");
    if (bodyStart == std::string::npos)
        return false;
    bodyStart += 4;

    // Strip trailing \r\n before the next boundary
    size_t bodyEnd = part.size();
    if (bodyEnd >= 2 && part[bodyEnd - 1] == '\n' && part[bodyEnd - 2] == '\r')
        bodyEnd -= 2;

    outFileContent = part.substr(bodyStart, bodyEnd - bodyStart);
    return true;
}

HttpResponse Handler::handleUpload(const RouteDecision& rd, const HttpRequest& req)
{

    if (rd.uploadPath.empty())
        return makeError(400, "No upload path configured");

    std::string filename, content;
    if (!parseMultipart(req, filename, content))
        return makeError(400, "Malformed multipart body");

    if (filename.empty())
        return makeError(400, "No filename provided");
    
    if (filename.find("..") != std::string::npos ||
            filename.find('/') != std::string::npos ||
            filename.find('\\') != std::string::npos)
    {
        return makeError(400, "Invalid filename");
    }

    std::string dest = rd.uploadPath;
    if (!dest.empty() && dest.back() != '/')//if the upload path doesn't already end with a slash
        dest += '/';//add a slash so we don't get double slashes
    dest += filename;//append the filename to the upload path to get the full destination path

    if (!FileSystem::writeFile(dest, content))
        return makeError(500, "Could not write upload");

    HttpResponse res;
    res.status = 201;
    res.reason = "Created";
    res.headers["Location"] = "/" + filename;
    res.headers["Content-Length"] = "0";
    return res;
}

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

HttpResponse Handler::makeErrorWithConfig(const RouteDecision& rd, int code, const std::string& message)
{
    auto it = rd.errorPages.find(code);
    if (it != rd.errorPages.end())
    {
        std::string content;
        if (FileSystem::readFile(it->second, content))
        {
            HttpResponse res;
            res.status = code;
            res.reason = message;
            res.body = content;
            res.headers["Content-Type"] = "text/html";
            return res;
        }
        // file path was configured but couldn't be read — fall through to default
    }
    return makeError(code, message);
}

HttpResponse Handler::handleCgi(const RouteDecision& rd, const HttpRequest& req,
    const std::string& fullPath)
{
    std::cerr << "DEBUG: START CGI\n";
    (void)rd; // unused parameter-- will be used in future for CGI-specific config
    std::cerr << "DEBUG: CGI fullPath = [" << fullPath << "]\n";
    std::cerr << "DEBUG: Exists = " << FileSystem::exists(fullPath) << "\n";
    if (!FileSystem::exists(fullPath))//if the CGI script file does not exist on disk
        return makeError(404, "Not Found");
    int inPipe[2];   // parent writes request body -> child reads (stdin)
    int outPipe[2];  // child writes response -> parent reads (stdout)
    if (pipe(inPipe) == -1 || pipe(outPipe) == -1)
        return makeError(500, "Could not create pipes");
    pid_t pid = fork();//fork duplicates the current process(so here ./webserver has a child)
    if (pid == -1)
        return makeError(500, "Fork failed");
    if (pid == 0)// ONLY CHILD process executes this block, parent process skips it and continues after the block
    {
        dup2(inPipe[0], STDIN_FILENO);//replace stdin with the read end of the input pipe, so that when the CGI script reads from stdin, it gets the request body from the parent process instead of the terminal
        dup2(outPipe[1], STDOUT_FILENO);//replace stdout with the write end of the output pipe, so that when the CGI script writes to stdout, it goes to the parent process instead of the terminal
        close(inPipe[0]); close(inPipe[1]);
        close(outPipe[0]); close(outPipe[1]);

        std::string contentLength = std::to_string(req.body.size());//convert the size of the request body to a string, which will be used as the value for the CONTENT_LENGTH environment variable
        std::vector<std::string> envStrings = {
                                                "REQUEST_METHOD=" + req.method,
                                                "CONTENT_LENGTH=" + contentLength,
                                                "SCRIPT_FILENAME=" + fullPath,
                                                "GATEWAY_INTERFACE=CGI/1.1",
                                                "SERVER_PROTOCOL=HTTP/1.1",
                                                "QUERY_STRING="
                                                };//create a vector of strings representing the environment variables that will be passed to the CGI script. Each string is in the format "KEY=VALUE", where KEY is the name of the environment variable and VALUE is its value. These variables provide information about the request and the server to the CGI script.
        std::vector<char*> envp;//execve() expects an array of C-style strings (char*), so we need to convert the std::string objects in envStrings to char* pointers
        for (auto& s : envStrings) envp.push_back(const_cast<char*>(s.c_str()));//convert each std::string in envStrings to a C-style string using c_str(), then cast away the constness with const_cast<char*> so that we can store it in the envp vector. This is necessary because execve() expects non-const char* pointers for the environment variables, even though we are not modifying them.
        envp.push_back(nullptr);

        char* argv[] = { const_cast<char*>(fullPath.c_str()), nullptr };
        std::cerr << "DEBUG::EXECUTING: " << fullPath << '\n';
        execve(fullPath.c_str(), argv, envp.data());//execve() replaces the current process image with a new process image specified by the CGI script file. It takes three arguments: the path to the executable (fullPath), an array of argument strings (argv), and an array of environment variable strings (envp). If execve() is successful, it does not return, and the CGI script starts executing. If it fails, it returns -1, and we handle the error by exiting the child process with a non-zero status code.
        // execve failed
        std::exit(1);
    }

    // Parent
    close(inPipe[0]);//no need to read from the input pipe in the parent, so we close the read end of the input pipe
    close(outPipe[1]);//no need to write to the output pipe in the parent, so we close the write end of the output pipe
    write(inPipe[1], req.body.c_str(), req.body.size());//write the request body to the write end of the input pipe, which will be read by the child process (the CGI script) as its stdin
    close(inPipe[1]);
    std::string output;
    char buf[4096];
    ssize_t n;
    while ((n = read(outPipe[0], buf, sizeof(buf))) > 0)
        output.append(buf, n);//store the output from the CGI script (read from the read end of the output pipe) in the output string, which will be used to construct the HttpResponse later
    close(outPipe[0]);
    int status;
    waitpid(pid, &status, 0);
    HttpResponse res;
    if (WIFEXITED(status) && WEXITSTATUS(status) == 0)//if the child process (the CGI script) exited normally and returned a status code of 0, indicating success
    {
        // CGI output is headers + blank line + body, same as raw HTTP
        size_t sep = output.find("\r\n\r\n");
        if (sep == std::string::npos)
        sep = output.find("\n\n");
        if (sep != std::string::npos)
        {
            res.body = output.substr(sep + (output[sep] == '\r' ? 4 : 2));
            res.headers["Content-Type"] = "text/html"; // CGI scripts may override via header parsing later
        }
        else
        {
            res.body = output;
            res.headers["Content-Type"] = "text/html";
        }
    }
    else
    {
        return makeError(502, "Bad Gateway");
    }
    std::cerr << "DEBUG::CGI DONE\n";
    return res;
}

// ─────────────────────────────────────────────
// Autoindex
// ─────────────────────────────────────────────
// Generates a simple HTML page listing the contents of a directory.
// The directory entries are sorted alphabetically.
// The generated HTML is very basic, but it includes links to each file/subdirectory.
// The links are constructed by appending the entry name to the request URI path.
// If the request URI path is not "/", a link to the parent directory ("../") is also included.
// The generated HTML is returned in an HttpResponse with a 200 OK status and a Content-Type of text/html.
// The function returns a 500 Internal Server Error if the directory cannot be read.
//

// Example usage:
// RouteDecision rd;
// rd.root = "./www";
// rd.autoindex = true;
// HttpRequest req;
// req.path = "/images";
// Handler handler;
// HttpResponse res = handler.handleAutoindex(rd.root + req.path, req.path);    

// Example output (for a directory containing "cat.jpg" and "dog.jpg"):
// <!DOCTYPE html>//starts the HTML document and specifies the HTML version
// <html>//starts the HTML document
// <head><title>Index of /images</title></head>//defines the head section of the HTML document, which contains metadata and the title of the page
// <body>
// <h1>Index of /images</h1>//displays a heading with the text "Index of /images"
// <hr>//inserts a horizontal rule to separate the heading from the list of directory entries
// <ul>//starts an unordered list to display the directory entries
//   <li><a href="../">../</a></li> //adds a list item with a link to the parent directory



HttpResponse Handler::handleAutoindex(const std::string& dirPath, const std::string& uriPath)
{
    std::vector<std::string> entries;//vector to hold the directory entries
    if (!FileSystem::isDir(dirPath))
        return makeError(404, "Not Found");
    if (!FileSystem::listDir(dirPath, entries))//check if the directory can be read
        return makeError(500, "Could not read directory");

    std::sort(entries.begin(), entries.end());//sort the directory entries alphabetically

    std::string html = "<!DOCTYPE html>\n<html>\n<head><title>Index of "
        + uriPath + "</title></head>\n<body>\n<h1>Index of " + uriPath
        + "</h1>\n<hr>\n<ul>\n";//start building the HTML response with the directory listing

    if (uriPath != "/")//if the request URI path is not "/", add a link to the parent directory
        html += "  <li><a href=\"../\">../</a></li>\n";

    for (const std::string& name : entries)
    {
        std::string href = uriPath;//href is the link to the directory entry, starting with the request URI path
        if (!href.empty() && href.back() != '/')//if the request URI path doesn't already end with a slash
            href += '/';
        href += name;
        html += "  <li><a href=\"" + href + "\">" + name + "</a></li>\n";//add a list item with a link to the directory entry
    }
    html += "</ul>\n<hr>\n</body>\n</html>\n";

    HttpResponse res;
    res.body = html;
    res.headers["Content-Type"] = "text/html";
    return res;
}
// ─────────────────────────────────────────────
// Static file (GET)
// ─────────────────────────────────────────────
HttpResponse Handler::handleStaticFile(const RouteDecision& rd, const std::string& fullPath)
{
    if (!FileSystem::exists(fullPath))
        return makeErrorWithConfig(rd, 404, "Not Found");

    std::string content;
    if (!FileSystem::readFile(fullPath, content))
        return makeErrorWithConfig(rd, 500, "Could not read file");

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
HttpResponse Handler::handleDelete(const RouteDecision& rd, const std::string& fullPath)
{
    if (!FileSystem::exists(fullPath))
        return makeErrorWithConfig(rd, 404, "Not Found");

    if (std::remove(fullPath.c_str()) != 0)
        return makeErrorWithConfig(rd, 500, "Could not delete file");

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
        return makeErrorWithConfig(rd, 405, "Method Not Allowed");
    std::string fullPath = buildPath(rd, req);
    if (!rd.cgiPass.empty() && req.method != "DELETE")
        return handleCgi(rd, req, fullPath);
    if (req.method == "POST" && !rd.uploadPath.empty())
        return handleUpload(rd, req);
    if (FileSystem::isDir(fullPath))//if the path is a directory we serve the index file if it exists, otherwise return 403
    {
        if (rd.autoindex)
            return handleAutoindex(fullPath, req.path);
        std::string withIndex = fullPath;
        if (withIndex.back() != '/')//if the directory path doesn't already end with a slash
            withIndex += '/';//add a slash so we don't get double slashes
        withIndex += rd.index;//append the index filename to the directory path

        if (FileSystem::isFileNormal(withIndex))
            return handleStaticFile(rd, withIndex);

        return makeErrorWithConfig(rd, 403, "Forbidden");
    }
    if (req.method == "GET")
        return handleStaticFile(rd, fullPath);
    if (req.method == "DELETE")
        return handleDelete(rd, fullPath);
    return makeErrorWithConfig(rd, 405, "Method Not Allowed");
}
