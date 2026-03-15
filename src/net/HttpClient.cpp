#include "HttpClient.h"
#include <atomic>
#include "util/Logger.h"
#include <curl/curl.h>

struct CancelCtx { std::atomic<bool>* flag; };

static int progressCallback(void* userdata, curl_off_t, curl_off_t, curl_off_t, curl_off_t) {
    auto* ctx = static_cast<CancelCtx*>(userdata);
    if (ctx && ctx->flag && ctx->flag->load()) return 1; // abort
    return 0;
}

static size_t writeCallback(void* ptr, size_t size, size_t nmemb, std::string* data) {
    data->append((char*)ptr, size * nmemb);
    return size * nmemb;
}

bool HttpClient::get(const std::string& url, std::string& result,
                    std::atomic<bool>* cancelFlag) {
    LOG_INFO("HTTP GET: %s", url.c_str());
    
    CURL* curl = curl_easy_init();
    if (!curl) {
        LOG_ERROR("curl_easy_init failed");
        return false;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &result);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);  // Connection timeout
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);         // Total timeout
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);         // CRITICAL for embedded
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "WiiUModStore/0.1");
    
    CancelCtx ctx{ cancelFlag };
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progressCallback);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &ctx);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    LOG_INFO("curl_easy_perform starting...");
    CURLcode res = curl_easy_perform(curl);
    LOG_INFO("curl_easy_perform returned: %d", res);
    
    curl_easy_cleanup(curl);
    
    if (res != CURLE_OK) {
        LOG_ERROR("curl error: %s", curl_easy_strerror(res));
        return false;
    }
    
    LOG_INFO("HTTP GET success, %zu bytes", result.size());
    return true;
}
