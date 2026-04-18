#include "ConfigParser.hpp"
#include "FileSystem.hpp"
#include <sstream>
#include <stdexcept>

// ─────────────────────────────────────────────
// tokenize()
//
// Turn the file text into a list of tokens.
// Whitespace separates tokens. { } and ; are always their own token.
// Comments (#...) are ignored.
// ─────────────────────────────────────────────
std::vector<std::string> ConfigParser::tokenize(const std::string& text)
{
    std::vector<std::string> tokens;
    std::string current;

    for (size_t i = 0; i < text.size(); ++i)
    {
        char c = text[i];

        // Skip comments: from # to end of line
        if (c == '#') 
        {
            while (i < text.size() && text[i] != '\n') ++i;
            continue;
        }

        // { } ; are always their own token
        if (c == '{' || c == '}' || c == ';') 
        {
            if (!current.empty()) { tokens.push_back(current); current.clear(); }
            tokens.push_back(std::string(1, c));
            continue;
        }

        // Whitespace ends the current token
        if (std::isspace(static_cast<unsigned char>(c))) {
            if (!current.empty()) { tokens.push_back(current); current.clear(); }
            continue;
        }

        current += c;
    }
    if (!current.empty()) tokens.push_back(current);
    return tokens;
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

        if (key == "root") {
            loc.root = tokens[i++];
        }
        else if (key == "index") {
            loc.index = tokens[i++];
        }
        else if (key == "autoindex") {
            loc.autoindex = (tokens[i++] == "on");
        }
        else if (key == "upload_dir") {
            loc.uploadPath = tokens[i++];
        }
        else if (key == "cgi") {
            loc.cgiPass = tokens[i++];
        }
        else if (key == "methods") {
            // Read words until ;
            while (i < tokens.size() && tokens[i] != ";")
                loc.methods.push_back(tokens[i++]);
        }
        else if (key == "return") {
            loc.redirectCode = std::stoi(tokens[i++]);
            loc.redirectUrl  = tokens[i++];
        }
        else {
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
        const std::string& key = tokens[i++];

        if (key == "listen") {
            srv.port = std::stoi(tokens[i++]);
        }
        else if (key == "root") {
            srv.root = tokens[i++];
        }
        else if (key == "index") {
            srv.index = tokens[i++];
        }
        else if (key == "location") {
            srv.locations.push_back(parseLocation(tokens, i));
            continue; // parseLocation consumed its own ;/}
        }
        else {
            throw std::runtime_error("unknown directive in server: " + key);
        }

        if (i < tokens.size() && tokens[i] == ";") ++i;
    }

    if (i >= tokens.size()) throw std::runtime_error("missing } for server");
    ++i;

    return srv;
}

// ─────────────────────────────────────────────
// parse()
//
// Top-level: read file, tokenize, find "server { ... }".
// ─────────────────────────────────────────────
ServerConfig ConfigParser::parse(const std::string& filename)
{
    std::string text;
    if (!FileSystem::readFile(filename, text))
        throw std::runtime_error("cannot open config file: " + filename);

    std::vector<std::string> tokens = tokenize(text);

    size_t i = 0;
    if (i >= tokens.size() || tokens[i] != "server")
        throw std::runtime_error("config must start with 'server'");
    ++i;

    return parseServer(tokens, i);
}