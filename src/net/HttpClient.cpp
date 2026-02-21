#include "HttpClient.h"
#include "util/Logger.h"

#include <curl/curl.h>
#include <string>

static size_t writeCallback(char* ptr, size_t size, size_t nmemb, std::string* data) {
    data->append(ptr, size * nmemb);
    return size * nmemb;
}

bool HttpClient::get(const std::string& url, std::string& result) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        LOG_ERROR("curl_easy_init failed");
        return false;
    }

    result.clear();

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &result);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L); // Wii U has no CA bundle
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "WiiUModStore/0.1");

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        LOG_ERROR("curl error: %s", curl_easy_strerror(res));
        return false;
    }

    return true;
}
