#include "DownloadManager.h"
#include "mods/ZipExtractor.h"
#include "util/Logger.h"

#include <curl/curl.h>
#include <sys/stat.h>
#include <cstdio>
#include <string>

static void mkdirp(const std::string& path) {
    for (size_t i = 1; i <= path.size(); i++) {
        if (i == path.size() || path[i] == '/') {
            std::string sub = path.substr(0, i);
            mkdir(sub.c_str(), 0755);
        }
    }
}

static size_t writeFile(char* ptr, size_t size, size_t nmemb, FILE* f) {
    return fwrite(ptr, size, nmemb, f);
}

int DownloadManager::curlProgress(void* userdata, curl_off_t total, curl_off_t now,
                                   curl_off_t, curl_off_t) {
    auto* dm = static_cast<DownloadManager*>(userdata);
    if (total > 0)
        dm->m_progress = (float)now / (float)total;
    return 0;
}

void DownloadManager::run(const std::string& zipUrl,
                           const std::string& tmpPath,
                           const std::string& destDir) {
    m_state    = State::Downloading;
    m_progress = 0.0f;
    m_error.clear();

    // Ensure cache dir exists (recursive)
    size_t slash = tmpPath.rfind('/');
    if (slash != std::string::npos)
        mkdirp(tmpPath.substr(0, slash));

    FILE* f = fopen(tmpPath.c_str(), "wb");
    if (!f) {
        m_error = "Cannot create: " + tmpPath;
        m_state = State::Error;
        return;
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        fclose(f);
        m_error = "curl_easy_init failed";
        m_state = State::Error;
        return;
    }

    curl_easy_setopt(curl, CURLOPT_URL,              zipUrl.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,    writeFile);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA,        f);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION,   1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT,          60L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER,   0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST,   0L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT,        "WiiUModStore/0.1");
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, curlProgress);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA,     this);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS,       0L);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    fclose(f);

    if (res != CURLE_OK) {
        m_error = std::string("Download failed: ") + curl_easy_strerror(res);
        m_state = State::Error;
        return;
    }

    m_state    = State::Extracting;
    m_progress = 0.0f;

    if (!ZipExtractor::extract(tmpPath, destDir)) {
        m_error = "Failed to extract ZIP";
        m_state = State::Error;
        return;
    }

    remove(tmpPath.c_str());
    m_progress = 1.0f;
    m_state    = State::Done;
    LOG_INFO("DownloadManager: done -> %s", destDir.c_str());
}
