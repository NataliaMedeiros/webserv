#pragma once
#include <string>
#include <vector>

// All filesystem operations the handlers need.
// Kept as static methods so no instance is needed.
class FileSystem
{
    public:
        // Does this path exist at all (file or directory)?
        static bool exists(const std::string& path);

        // Is this path a regular file?(not a directory, symbolic link, or special file)
        static bool isFileNormal(const std::string& path);

        // Is this path a directory?
        static bool isDir(const std::string& path);

        // Read entire file into a string. Returns false if failed.
        static bool readFile(const std::string& path, std::string& outContent);

        // List filenames inside a directory (not recursive).
        // Returns false if path is not a directory or can't be opened.
        static bool listDir(const std::string& path, std::vector<std::string>& outEntries);

        // Write content to file. Returns false if failed.
        static bool writeFile(const std::string& path, const std::string& content);

        // Map file extension to MIME type.
        static std::string mimeType(const std::string& path);
};
