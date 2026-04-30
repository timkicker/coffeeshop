#pragma once
#include <string>
#include <atomic>
#include <curl/curl.h>
class HttpClient {
public:
    // Fetches URL content into result string.
    // Returns true on success, false on error.
    // If cancelFlag is set to true during the request, curl aborts immediately.
    static bool get(const std::string& url, std::string& result,
                    std::atomic<bool>* cancelFlag = nullptr);
};
