#include "curl_mock.h"

#include <curl/curl.h>
#include <cstdarg>
#include <cstring>
#include <map>
#include <string>

// Per-handle state captured from curl_easy_setopt calls.
struct MockHandle {
    std::string url;
    size_t (*writeFunc)(char*, size_t, size_t, void*) = nullptr;
    void*    writeData = nullptr;
    long     responseCode = 0;
};

namespace {

struct Mapping {
    bool        isError = false;
    int         curlCode = 0;
    long        httpCode = 200;
    std::string body;
};

static std::map<std::string, Mapping> g_responses;
static int g_performCount = 0;

} // namespace

namespace CurlMock {

void reset() {
    g_responses.clear();
    g_performCount = 0;
}

void setResponse(const std::string& url, const std::string& body, long httpCode) {
    Mapping m;
    m.body = body;
    m.httpCode = httpCode;
    g_responses[url] = m;
}

void setError(const std::string& url, int curlCode) {
    Mapping m;
    m.isError = true;
    m.curlCode = curlCode;
    g_responses[url] = m;
}

int performCount() { return g_performCount; }

} // namespace CurlMock

extern "C" {

CURL* curl_easy_init() {
    return reinterpret_cast<CURL*>(new MockHandle());
}

void curl_easy_cleanup(CURL* curl) {
    delete reinterpret_cast<MockHandle*>(curl);
}

CURLcode curl_easy_setopt(CURL* curl, CURLoption option, ...) {
    if (!curl) return CURLE_FAILED_INIT;
    auto* h = reinterpret_cast<MockHandle*>(curl);
    va_list args;
    va_start(args, option);

    switch (option) {
        case CURLOPT_URL: {
            const char* url = va_arg(args, const char*);
            if (url) h->url = url;
            break;
        }
        case CURLOPT_WRITEFUNCTION: {
            void* fp = va_arg(args, void*);
            h->writeFunc = reinterpret_cast<size_t(*)(char*, size_t, size_t, void*)>(fp);
            break;
        }
        case CURLOPT_WRITEDATA: {
            h->writeData = va_arg(args, void*);
            break;
        }
        default:
            // Consume one argument and ignore - all the curl options we don't
            // care about take a single arg of varying type.
            (void)va_arg(args, void*);
            break;
    }

    va_end(args);
    return CURLE_OK;
}

CURLcode curl_easy_perform(CURL* curl) {
    g_performCount++;
    if (!curl) return CURLE_FAILED_INIT;
    auto* h = reinterpret_cast<MockHandle*>(curl);

    auto it = g_responses.find(h->url);
    if (it == g_responses.end()) {
        // No mapping → simulate connection failure
        return CURLE_COULDNT_CONNECT;
    }

    const auto& m = it->second;
    if (m.isError) {
        h->responseCode = 0;
        return static_cast<CURLcode>(m.curlCode);
    }

    h->responseCode = m.httpCode;
    if (m.httpCode >= 400) {
        // HTTP error: still feed the body but most callers check httpCode after
        // perform returns CURLE_OK. We follow real curl: return OK, let the
        // caller see the 4xx via getinfo.
    }

    if (h->writeFunc && !m.body.empty()) {
        h->writeFunc(const_cast<char*>(m.body.data()), 1, m.body.size(), h->writeData);
    }
    return CURLE_OK;
}

CURLcode curl_easy_getinfo(CURL* curl, CURLINFO info, ...) {
    if (!curl) return CURLE_FAILED_INIT;
    auto* h = reinterpret_cast<MockHandle*>(curl);
    va_list args;
    va_start(args, info);

    if (info == CURLINFO_RESPONSE_CODE) {
        long* out = va_arg(args, long*);
        if (out) *out = h->responseCode;
    } else {
        (void)va_arg(args, void*);
    }

    va_end(args);
    return CURLE_OK;
}

const char* curl_easy_strerror(CURLcode c) {
    switch (c) {
        case CURLE_OK: return "OK";
        case CURLE_COULDNT_CONNECT: return "Could not connect";
        case CURLE_COULDNT_RESOLVE_HOST: return "Could not resolve host";
        default: return "mock error";
    }
}

curl_slist* curl_slist_append(curl_slist* list, const char*) { return list; }
void        curl_slist_free_all(curl_slist*) {}

} // extern "C"
