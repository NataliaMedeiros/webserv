#include "ConfigParser.hpp"
#include "FileSystem.hpp"
#include <sstream>
#include <stdexcept>
#include <iostream>

// ─────────────────────────────────────────────
// tokenize()
//
// Turn the file text into a list of tokens.
// Whitespace separates tokens. { } and ; are always their own token.
// Comments (#...) are ignored.
// So tokens looks like: ["server", "{", "listen", "80", ";", "}"]
// ─────────────────────────────────────────────
std::vector<std::string> ConfigParser::tokenize(const std::string& text)
{
    std::vector<std::string> tokens; //vector is a dynamic array to store tokens
    std::string temp;//temporary storage for building up a token

    for (size_t i = 0; i < text.size(); ++i)
    {
        char c = text[i];
        if (c == '#') 
        {
            while (i < text.size() && text[i] != '\n') ++i;
            continue;// Skip comments: from # to end of line
        }
        if (c == '{' || c == '}' || c == ';')  // { } ; are always their own token
        {
            if (!temp.empty()) //if there is a token leftover
            { tokens.push_back(temp); temp.clear(); } //push it to vector and clear temp for next token
            tokens.push_back(std::string(1, c)); //take the symbol as its own token
            continue;
        }
        // Whitespace ends the temp token
        if (std::isspace(static_cast<unsigned char>(c))) 
        {
            if (!temp.empty()) { tokens.push_back(temp); temp.clear(); }
            continue;
        }
        temp += c;//build up the token character by character
    }
    if (!temp.empty()) tokens.push_back(temp);
    return tokens;
}
// ─────────────────────────────────────────────
// parseListen()
// Accepts either "8080" or "127.0.0.1:8080"
// ─────────────────────────────────────────────
static void parseListen(const std::string& value, ServerConfig& srv)
{
    size_t colon = value.find(':');
    if (colon == std::string::npos)
    {
        // Just a port, e.g. "listen 8080;"
        try { srv.port = std::stoi(value); }
        catch (...) { throw std::runtime_error("invalid port: " + value); }
    }
    else
    {
        // "host:port", e.g. "listen 127.0.0.1:8080;"
        srv.host = value.substr(0, colon);
        std::string portStr = value.substr(colon + 1);
        try { srv.port = std::stoi(portStr); }
        catch (...) { throw std::runtime_error("invalid port: " + portStr); }
    }

    if (srv.port < 1 || srv.port > 65535)
        throw std::runtime_error("port out of range: " + std::to_string(srv.port));
}
// ─────────────────────────────────────────────
// parseSize()
// Accepts plain bytes ("1000000") or suffixed ("10M", "500K")
// ─────────────────────────────────────────────
static size_t parseSize(const std::string& value)
{
    if (value.empty())
        throw std::runtime_error("empty client_max_body_size value");

    char suffix = value.back();
    size_t multiplier = 1;
    std::string numPart = value;

    if (suffix == 'K' || suffix == 'k') { multiplier = 1024; numPart = value.substr(0, value.size() - 1); }
    else if (suffix == 'M' || suffix == 'm') { multiplier = 1024 * 1024; numPart = value.substr(0, value.size() - 1); }
    else if (suffix == 'G' || suffix == 'g') { multiplier = 1024 * 1024 * 1024; numPart = value.substr(0, value.size() - 1); }

    try { return static_cast<size_t>(std::stoul(numPart)) * multiplier; }
    catch (...) { throw std::runtime_error("invalid client_max_body_size: " + value); }
}

// ─────────────────────────────────────────────
// parseLocation()
//
// Called after we've seen "location <path> {".
// Reads directives until we hit "}".
// ─────────────────────────────────────────────
LocationConfig ConfigParser::parseLocation(std::vector<std::string>& tokens, size_t& i)
{
    LocationConfig loc;

    // Expect: location <path> {
    if (i >= tokens.size()) throw std::runtime_error("expected location path");
    loc.path = tokens[i++];

    if (i >= tokens.size() || tokens[i] != "{")
        throw std::runtime_error("expected { after location path");
    ++i;

    // Read directives until }
    while (i < tokens.size() && tokens[i] != "}")
    {
        const std::string& key = tokens[i++];

        if (key == "root") 
        {
            loc.root = tokens[i++];//assign the next token to root and move to next token
            if (i >= tokens.size() || tokens[i] != ";")
                throw std::runtime_error("missing ; after root");
        }
        else if (key == "index") 
        {
            loc.index = tokens[i++];
            if (i >= tokens.size() || tokens[i] != ";")
                throw std::runtime_error("missing ; after index");
        }
        else if (key == "autoindex")
        {
            loc.autoindex = (tokens[i] == "on" || tokens[i] == "true" || tokens[i] == "1");
            ++i;
            if (i >= tokens.size() || tokens[i] != ";")
                throw std::runtime_error("missing ; after autoindex");
        }
        else if (key == "upload_dir")
        {
            loc.uploadPath = tokens[i++];
            if (i >= tokens.size() || tokens[i] != ";")
                throw std::runtime_error("missing ; after upload_dir");
        }
        else if (key == "cgi") 
        {
            loc.cgiPass = tokens[i++];

            if (i >= tokens.size() || tokens[i] != ";")
                throw std::runtime_error("missing ; after cgi");
        }
        else if (key == "cgi_ext") 
        {
            loc.cgiExtension = tokens[i++];
            std::cout << "cgi_extension tokens size=" << tokens.size()
                        << " i=" << i << std::endl;
            if (i >= tokens.size() || tokens[i] != ";")
                throw std::runtime_error("missing ; after cgi_ext");
        }
        else if (key == "methods") 
        {
            // Read words until ;
            while (i < tokens.size() && tokens[i] != ";" && tokens[i] != "}")
                loc.methods.push_back(tokens[i++]); //add the method to the methods vector and move to next token
            if (i >= tokens.size() || tokens[i] != ";")
                throw std::runtime_error("missing ; after methods");
        }
        else if (key == "error_page")
        {
            int code;
            try { code = std::stoi(tokens[i++]); }
            catch (...) { throw std::runtime_error("invalid error_page code: " + tokens[i-1]); }
            if (i >= tokens.size() || tokens[i] == ";" || tokens[i] == "}")
                throw std::runtime_error("error_page missing file path");
            loc.errorPages[code] = tokens[i++];
            if (i >= tokens.size() || tokens[i] != ";")
                throw std::runtime_error("missing ; after error_page");
        }
        else if (key == "return") 
        {
            
            try { loc.redirectCode = std::stoi(tokens[i++]); }//stoi() converts a string to an integer;assign the next token to port and move to next token
            catch (...) { throw std::runtime_error("invalid redirectCode: " + tokens[i-1]); }
            if (i >= tokens.size() || tokens[i] == ";" || tokens[i] == "}")//no URL provided after code
                throw std::runtime_error("return directive missing URL");
            loc.redirectUrl  = tokens[i++];
            if (i >= tokens.size() || tokens[i] != ";")
                throw std::runtime_error("missing ; after return");
        }
         else if (key == "client_max_body_size")
        {
            loc.maxBodySize = parseSize(tokens[i++]);
            if (i >= tokens.size() || tokens[i] != ";")
                throw std::runtime_error("missing ; after client_max_body_size");
        }
        else 
        {
            throw std::runtime_error("unknown directive in location: " + key);
        }

        // Skip the ; after each directive
        if (i < tokens.size() && tokens[i] == ";") ++i;
    }

    if (i >= tokens.size()) throw std::runtime_error("missing } for location");
    ++i; // consume }

    return loc;
}

// ─────────────────────────────────────────────
// parseServer()
//
// Called after we've seen "server {".
// Reads directives and nested location blocks until "}".
// ─────────────────────────────────────────────
ServerConfig ConfigParser::parseServer(std::vector<std::string>& tokens, size_t& i)
{
    ServerConfig srv;
    if (i >= tokens.size() || tokens[i] != "{")
        throw std::runtime_error("expected { after server");
    ++i;

    while (i < tokens.size() && tokens[i] != "}")
    {
        const std::string& key = tokens[i++];//assign the current token to key and move to next token

        if (key == "listen")
        {
            std::string value = tokens[i++];
            parseListen(value, srv);
            if (i >= tokens.size() || tokens[i] != ";")
                throw std::runtime_error("missing ; after listen");
        }
        else if (key == "root") 
        {
            srv.root = tokens[i++];
            if (i >= tokens.size() || tokens[i] != ";")
                throw std::runtime_error("missing ; after root");
        }
        else if (key == "index") 
        {
            srv.index = tokens[i++];
            if (i >= tokens.size() || tokens[i] != ";")
                throw std::runtime_error("missing ; after index");
        }
        else if (key == "location") 
        {
            srv.locations.push_back(parseLocation(tokens, i));
            continue; // parseLocation consumed its own ;/}
        }
        else if (key == "error_page")
        {
            int code;
            try { code = std::stoi(tokens[i++]); }
            catch (...) { throw std::runtime_error("invalid error_page code: " + tokens[i-1]); }
            if (i >= tokens.size() || tokens[i] == ";" || tokens[i] == "}")
                throw std::runtime_error("error_page missing file path");
            srv.errorPages[code] = tokens[i++];
            if (i >= tokens.size() || tokens[i] != ";")
                throw std::runtime_error("missing ; after error_page");
        }
         else if (key == "client_max_body_size")
        {
            srv.maxBodySize = parseSize(tokens[i++]);
            if (i >= tokens.size() || tokens[i] != ";")
                throw std::runtime_error("missing ; after client_max_body_size");
        }
        else if (key == "location")
        {
            srv.locations.push_back(parseLocation(tokens, i));
            continue;
        }
        else 
        {
            throw std::runtime_error("unknown directive in server: " + key);
        }
        if (i < tokens.size() && tokens[i] == ";") ++i;
    }
    if (i >= tokens.size()) throw std::runtime_error("missing } for server");
    ++i; // consume }
    return srv;
}

// ─────────────────────────────────────────────
// parse()
//
// Top-level: read file, tokenize, find "server { ... }".
// ─────────────────────────────────────────────
std::vector<ServerConfig> ConfigParser::parse(const std::string& filename)
{
    std::string text;
    if (!FileSystem::isFileNormal(filename))
        throw std::runtime_error("not a valid config file: " + filename);
    if (!FileSystem::readFile(filename, text))
        throw std::runtime_error("cannot open config file: " + filename);

    std::vector<std::string> tokens = tokenize(text);
    std::vector<ServerConfig> servers;
    size_t i = 0;
    while (i < tokens.size())
    {
        if (tokens[i] != "server")
            throw std::runtime_error("expected 'server' block, found: " + tokens[i]);
        ++i;
        servers.push_back(parseServer(tokens, i));
    }
    if (servers.empty())
        throw std::runtime_error("config must contain at least one server block");
    return servers;
}