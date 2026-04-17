#include "FileSystem.hpp"
#include <fstream>
#include <sstream>
#include <dirent.h>
#include <sys/stat.h>

//PURPOSE: Does this path exist at all (file or directory)?
//struct st is filled with metadata by stat(); path is converted to C string by c_str() for stat() call
//if file exists, stat() returns 0, otherwise -1
bool FileSystem::exists(const std::string& path)
{
    struct stat st; 
    return stat(path.c_str(), &st) == 0;
}


//PURPOSE: Is this path a regular file?(not a directory, symbolic link, or special file)
//if stat() return not zero, it means file does not exist
//if stat() return zero, we check if st_mode indicates it's a regular file using S_ISREG macro
//S_ISREG is a c++11 marco from sys/stat.h that checks if the file mode indicates a regular file

bool FileSystem::isFileNormal(const std::string& path)
{
    struct stat st;
    if (stat(path.c_str(), &st) != 0)
        return false;
    return S_ISREG(st.st_mode);//S_ISREG does the actual check
}

// Is this path a directory?
bool FileSystem::isDir(const std::string& path)
{
    struct stat st;
    if (stat(path.c_str(), &st) != 0)
        return false;//false if stat() fails, meaning file does not exist
    return S_ISDIR(st.st_mode);
}

//ifstream reads raw bytes. ios::binary prevents text mode translation for non-text files like images, PDFs
//ostringstream is used to read the entire file content into a string in one go
//rdbuf() exposes the underlying stream buffer of the file, (which is a pointer to char array containing the file content)
// << operator copies bytes from that buffer into the stringstream,
//which is then converted to a string from the string stream using ss.str() and stored in outContent

bool FileSystem::readFile(const std::string& path, std::string& outContent)
{
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open())
        return false;
    std::ostringstream ss;
    ss << file.rdbuf();
    outContent = ss.str();
    return true;
}


//struct dirent represents a directory entry,(defined in dirent.h)
// which contains metadata about a file or subdirectory within a directory.
//outEntries is a vector of strings that will be filled with the names of the entries
//in the directory specified by path

// List filenames inside a directory (not recursive).
// Returns false if path is not a directory or can't be opened.
bool FileSystem::listDir(const std::string& path, std::vector<std::string>& outEntries)
{
    DIR* dir = opendir(path.c_str());
    if (!dir)
        return false;
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr)
    {
        std::string name = entry->d_name;
        if (name == "." || name == "..")
            continue;//skip dots representing current and parent directory
        outEntries.push_back(name);
    }
    closedir(dir);
    return true;
}


// Write content to file. Returns false if failed.
bool FileSystem::writeFile(const std::string& path, const std::string& content)
{
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open())
        return false;
    file.write(content.c_str(), content.size());
    return file.good();
}

//This function extracts the file extension from the path and maps it to a corresponding MIME type string
//MIME = Multipurpose Internet Mail Extensions
//rfind() finds the last occurrence of a character in a string
//application/octet-stream is a generic MIME type for "tyoe not found" binary data

std::string FileSystem::mimeType(const std::string& path)
{
    size_t dot = path.rfind('.');
    if (dot == std::string::npos)//npos means dot not found
        return "application/octet-stream";
    std::string ext = path.substr(dot);
    if (ext == ".html" || ext == ".htm") return "text/html";
    if (ext == ".css")                   return "text/css";
    if (ext == ".js")                    return "application/javascript";
    if (ext == ".json")                  return "application/json";
    if (ext == ".txt")                   return "text/plain";
    if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
    if (ext == ".png")                   return "image/png";
    if (ext == ".gif")                   return "image/gif";
    if (ext == ".ico")                   return "image/x-icon";
    if (ext == ".pdf")                   return "application/pdf";
    return "application/octet-stream";
}
