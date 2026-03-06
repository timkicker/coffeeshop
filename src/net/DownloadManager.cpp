#include "DownloadManager.h"
#include "mods/ZipExtractor.h"
#include "util/Logger.h"

#include <curl/curl.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <dirent.h>
#include <string>
#include <vector>

// Minimum free space required: 64 MB
static constexpr uint64_t MIN_FREE_BYTES = 64 * 1024 * 1024;

// ZIP local file signature
static constexpr uint32_t ZIP_SIGNATURE = 0x04034b50;

static void mkdirp(const std::string& path) {
    for (size_t i = 1; i <= path.size(); i++) {
        if (i == path.size() || path[i] == '/')
            mkdir(path.substr(0, i).c_str(), 0755);
    }
}

static bool rmrf(const std::string& path) {
    // Simple recursive delete for cleanup on failed extract
    struct stat st;
    if (stat(path.c_str(), &st) != 0) return true;
    if (!S_ISDIR(st.st_mode)) { remove(path.c_str()); return true; }
    // For directories: use system rm -rf equivalent via posix
    // On WUT we don't have system(), so just remove the dir itself
    // ZipExtractor creates flat structure so this is usually enough
    rmdir(path.c_str());
    return true;
}

static size_t writeFile(char* ptr, size_t size, size_t nmemb, FILE* f) {
    return fwrite(ptr, size, nmemb, f);
}

int DownloadManager::curlProgress(void* userdata, curl_off_t total, curl_off_t now,
                                   curl_off_t, curl_off_t) {
    auto* dm = static_cast<DownloadManager*>(userdata);
    if (dm->m_cancelFlag && dm->m_cancelFlag->load()) return 1;
    if (total > 0)
        dm->m_progress = (float)now / (float)total;
    return 0;
}

bool DownloadManager::checkDiskSpace(const std::string& dir, uint64_t needed) {
    struct statvfs sv;
    if (statvfs(dir.c_str(), &sv) != 0) {
        LOG_WARN("DownloadManager: statvfs failed for %s, skipping space check", dir.c_str());
        return true; // Can't check, assume ok
    }
    uint64_t free = (uint64_t)sv.f_bavail * sv.f_frsize;
    if (free < needed) {
        LOG_ERROR("DownloadManager: not enough space: %llu free, %llu needed",
                  (unsigned long long)free, (unsigned long long)needed);
        return false;
    }
    return true;
}

bool DownloadManager::validateZip(const std::string& path) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return false;

    // Check file size > 0
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size < 4) { fclose(f); return false; }

    // Check ZIP magic bytes
    uint8_t magic[4];
    fread(magic, 1, 4, f);
    fclose(f);

    uint32_t sig = magic[0] | (magic[1]<<8) | (magic[2]<<16) | (magic[3]<<24);
    if (sig != ZIP_SIGNATURE) {
        LOG_ERROR("DownloadManager: invalid ZIP signature: 0x%08X", sig);
        return false;
    }
    return true;
}

bool DownloadManager::download(const std::string& zipUrl, const std::string& tmpPath) {
    // Ensure cache dir
    size_t slash = tmpPath.rfind('/');
    if (slash != std::string::npos) mkdirp(tmpPath.substr(0, slash));

    FILE* f = fopen(tmpPath.c_str(), "wb");
    if (!f) {
        m_error = "Cannot create temp file: " + tmpPath;
        return false;
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        fclose(f);
        m_error = "curl_easy_init failed";
        return false;
    }

    curl_easy_setopt(curl, CURLOPT_URL,              zipUrl.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,    writeFile);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA,        f);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION,   1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT,          120L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT,   15L);
    // Abort if below 1KB/s for 30 seconds
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT,  1024L);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME,   30L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER,   0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST,   0L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT,        "WiiUModStore/0.1");
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, curlProgress);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA,     this);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS,       0L);

    CURLcode res = curl_easy_perform(curl);
    long httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    curl_easy_cleanup(curl);
    fclose(f);

    if (res != CURLE_OK) {
        remove(tmpPath.c_str());
        m_error = std::string("Download failed: ") + curl_easy_strerror(res);
        return false;
    }
    if (httpCode >= 400) {
        remove(tmpPath.c_str());
        m_error = "HTTP error: " + std::to_string(httpCode);
        return false;
    }
    return true;
}

void DownloadManager::run(const std::string& zipUrl,
                           const std::string& tmpPath,
                           const std::string& destDir,
                           int maxRetries) {
    m_state    = State::Downloading;
    m_progress = 0.0f;
    m_error.clear();

    // Disk space check
    {
        size_t slash = tmpPath.rfind('/');
        std::string cacheDir = slash != std::string::npos ? tmpPath.substr(0, slash) : ".";
        mkdirp(cacheDir);
        if (!checkDiskSpace(cacheDir, MIN_FREE_BYTES)) {
            m_error = "Not enough disk space (need 64MB free)";
            m_state = State::Error;
            return;
        }
    }

    // Download with retry
    bool downloaded = false;
    for (int attempt = 1; attempt <= maxRetries + 1; attempt++) {
        m_progress = 0.0f;
        LOG_INFO("DownloadManager: attempt %d/%d: %s", attempt, maxRetries+1, zipUrl.c_str());
        if (download(zipUrl, tmpPath)) { downloaded = true; break; }
        LOG_WARN("DownloadManager: attempt %d failed: %s", attempt, m_error.c_str());
    }

    if (!downloaded) {
        m_state = State::Error;
        return;
    }

    // Validate ZIP
    if (!validateZip(tmpPath)) {
        remove(tmpPath.c_str());
        m_error = "Downloaded file is not a valid ZIP";
        m_state = State::Error;
        return;
    }

    // Extract
    m_state    = State::Extracting;
    m_progress = 0.0f;

    mkdirp(destDir);

    if (!ZipExtractor::extract(tmpPath, destDir)) {
        // Cleanup partial extract
        remove(tmpPath.c_str());
        rmrf(destDir);
        m_error = "Failed to extract ZIP";
        m_state = State::Error;
        return;
    }

    // Verify extract produced at least one file
    {
        int fileCount = 0;
        DIR* d = opendir(destDir.c_str());
        if (d) {
            struct dirent* e;
            while ((e = readdir(d)) != nullptr) {
                std::string n = e->d_name;
                if (n != "." && n != "..") { fileCount++; break; }
            }
            closedir(d);
        }
        if (fileCount == 0) {
            rmrf(destDir);
            m_error = "Extract produced no files";
            m_state = State::Error;
            return;
        }
    }

    remove(tmpPath.c_str());
    m_progress = 1.0f;
    m_state    = State::Done;
    LOG_INFO("DownloadManager: done -> %s", destDir.c_str());
}
