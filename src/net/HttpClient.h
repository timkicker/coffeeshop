#pragma once
#include <string>

class HttpClient {
public:
    // Fetches URL content into result string.
    // Returns true on success, false on error.
    static bool get(const std::string& url, std::string& result);
};
