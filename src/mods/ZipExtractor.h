#pragma once
#include <string>

class ZipExtractor {
public:
    // Extract a ZIP file to a destination directory.
    // Returns true on success.
    static bool extract(const std::string& zipPath, const std::string& destDir);

private:
    static bool ensureDir(const std::string& path);
};
