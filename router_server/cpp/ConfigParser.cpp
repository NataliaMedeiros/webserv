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
        else if (key == "autoindex") //not yet implemented
        {
            loc.autoindex = (tokens[i++] == "on");
            if (i >= tokens.size() || tokens[i] != ";")
                throw std::runtime_error("missing ; after autoindex");
        }
        else if (key == "upload_dir") //not yet implemented
        {
            loc.uploadPath = tokens[i++];
            if (i >= tokens.size() || tokens[i] != ";")
                throw std::runtime_error("missing ; after upload_dir");
        }
        else if (key == "cgi") //not yet implemented
        {
            loc.cgiPass = tokens[i++];
            if (i >= tokens.size() || tokens[i] != ";")
                throw std::runtime_error("missing ; after cgi");
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
            try { srv.port = std::stoi(tokens[i++]); }//stoi() converts a string to an integer;assign the next token to port and move to next token
            catch (...) { throw std::runtime_error("invalid port number: " + tokens[i-1]); }
            if (srv.port < 1 || srv.port > 65535)//valid port numbers are between 1 and 65535
                throw std::runtime_error("port out of range: " + std::to_string(srv.port));
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
ServerConfig ConfigParser::parse(const std::string& filename)
{
    std::string text;
    if (!FileSystem::isFileNormal(filename))
        throw std::runtime_error("not a valid config file: " + filename);
    if (!FileSystem::readFile(filename, text))
        throw std::runtime_error("cannot open config file: " + filename);

    std::vector<std::string> tokens = tokenize(text);
    size_t i = 0;
    if (i >= tokens.size() || tokens[i] != "server")
        throw std::runtime_error("config must start with 'server'");
    ++i;
    return parseServer(tokens, i);
}