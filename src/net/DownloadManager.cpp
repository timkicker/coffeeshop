#include "DownloadManager.h"
#include "mods/ZipExtractor.h"
#include "util/Logger.h"
#include "util/sha256.h"

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

extern void elogf(const char* fmt, ...);

#include <set>
#include <chrono>
namespace {
struct DmRmrfKey { dev_t dev; ino_t ino; };
inline bool operator<(const DmRmrfKey& a, const DmRmrfKey& b) {
    if (a.dev != b.dev) return a.dev < b.dev;
    return a.ino < b.ino;
}

struct DmRmrfState {
    std::set<DmRmrfKey> visited;
    std::chrono::steady_clock::time_point deadline;
    bool                                  timedOut = false;
};

// 30-second budget per top-level rmrf. Enough for any reasonable mod folder
// on slow SD; aborts before the user perceives a hang.
static constexpr int kRmrfBudgetSec = 30;

static bool dm_rmrf_impl(const std::string& path, DmRmrfState& state, int depth) {
    if (state.timedOut) return false;
    if (std::chrono::steady_clock::now() > state.deadline) {
        if (!state.timedOut) {
            elogf("rmrf: TIMEOUT after %ds at %s -- abort", kRmrfBudgetSec, path.c_str());
            state.timedOut = true;
        }
        return false;
    }
    if (depth > 64) {
        elogf("rmrf: depth>64 at %s -- abort", path.c_str());
        return false;
    }
    struct stat st;
    if (lstat(path.c_str(), &st) != 0) return true; // already gone
    if (!S_ISDIR(st.st_mode)) {
        // file or symlink -- remove without following
        return remove(path.c_str()) == 0;
    }
    DmRmrfKey key{st.st_dev, st.st_ino};
    if (!state.visited.insert(key).second) {
        elogf("rmrf: cycle at %s -- abort", path.c_str());
        return false;
    }
    DIR* d = opendir(path.c_str());
    if (!d) {
        elogf("rmrf: opendir failed for %s", path.c_str());
        return false;
    }
    bool ok = true;
    int childCount = 0;
    struct dirent* e;
    while ((e = readdir(d)) != nullptr) {
        std::string name = e->d_name;
        if (name == "." || name == "..") continue;
        childCount++;
        if (!dm_rmrf_impl(path + "/" + name, state, depth + 1)) ok = false;
        if (state.timedOut) break;
    }
    closedir(d);
    if (state.timedOut) return false;
    if (depth <= 1)
        elogf("rmrf: %s had %d children", path.c_str(), childCount);
    if (rmdir(path.c_str()) != 0) {
        elogf("rmrf: rmdir failed for %s", path.c_str());
        ok = false;
    }
    return ok;
}
} // namespace

bool DownloadManager::rmrf(const std::string& path) {
    elogf("rmrf BEGIN %s", path.c_str());
    DmRmrfState state;
    state.deadline = std::chrono::steady_clock::now() +
                     std::chrono::seconds(kRmrfBudgetSec);
    bool result = dm_rmrf_impl(path, state, 0);
    elogf("rmrf END %s -> %d (timedOut=%d)", path.c_str(), (int)result, (int)state.timedOut);
    return result;
}

static size_t writeFile(char* ptr, size_t size, size_t nmemb, FILE* f) {
    size_t written = fwrite(ptr, size, nmemb, f);
    if (written != nmemb) {
        // Short write: I/O error (likely SD full or unmounted). curl will see
        // the mismatch and abort with CURLE_WRITE_ERROR.
        LOG_ERROR("DownloadManager: short write %zu/%zu (errno=%d)",
                  written, nmemb, ferror(f) ? ferror(f) : 0);
    }
    return written;
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
    // Wii U statvfs doesn't understand the "fs:/" mount prefix. Try the
    // raw path first, then strip the prefix. If both fail, log once at
    // INFO level (not WARN -- this is expected on hardware) and proceed.
    struct statvfs sv;
    int rc = statvfs(dir.c_str(), &sv);
    if (rc != 0 && dir.compare(0, 3, "fs:") == 0) {
        std::string stripped = dir.substr(3);
        rc = statvfs(stripped.c_str(), &sv);
    }
    if (rc != 0) {
        static bool warned = false;
        if (!warned) {
            LOG_INFO("DownloadManager: statvfs not supported on this platform, skipping disk-space checks");
            warned = true;
        }
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
    if (fread(magic, 1, 4, f) != 4) { fclose(f); return false; }
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

    // Resume support: download to tmpPath + ".partial", rename on success.
    // If a .partial exists from a previous attempt, use HTTP Range to continue.
    std::string partialPath = tmpPath + ".partial";
    long resumeFrom = 0;
    {
        struct stat st;
        if (stat(partialPath.c_str(), &st) == 0 && st.st_size > 0) {
            resumeFrom = (long)st.st_size;
            LOG_INFO("DownloadManager: resuming from %ld bytes", resumeFrom);
        }
    }

    FILE* f = fopen(partialPath.c_str(), resumeFrom > 0 ? "ab" : "wb");
    if (!f) {
        m_error = "Cannot create temp file: " + partialPath;
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
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT,   15L);
    // Abort if below 100 bytes/s for 60 seconds
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT,  100L);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME,   60L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER,   0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST,   0L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT,        "WiiUModStore/0.1");
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, curlProgress);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA,     this);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS,       0L);
    if (resumeFrom > 0) {
        curl_easy_setopt(curl, CURLOPT_RESUME_FROM_LARGE, (curl_off_t)resumeFrom);
    }

    CURLcode res = curl_easy_perform(curl);
    long httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    curl_easy_cleanup(curl);
    fclose(f);

    if (res != CURLE_OK) {
        // Don't delete .partial -- next retry can resume from what we got
        m_error = std::string("Download failed: ") + curl_easy_strerror(res);
        return false;
    }
    if (httpCode >= 400) {
        // 4xx is permanent (file gone, auth) -- discard partial
        remove(partialPath.c_str());
        m_error = "HTTP error: " + std::to_string(httpCode);
        return false;
    }
    // Atomically rename .partial -> .zip
    if (rename(partialPath.c_str(), tmpPath.c_str()) != 0) {
        m_error = "Cannot finalize download (rename .partial failed)";
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
        if (m_cancelFlag && m_cancelFlag->load()) break;
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

    // SHA-256 verification (opt-in via Mod.sha256 in repo)
    if (!m_expectedHash.empty()) {
        m_state = State::Verifying;
        m_progress = 0.0f;
        LOG_INFO("DownloadManager: verifying SHA-256 (%zu bytes expected)",
                 m_expectedHash.size());
        if (!sha256VerifyFile(tmpPath, m_expectedHash)) {
            std::string actual = sha256HexFile(tmpPath);
            LOG_ERROR("DownloadManager: SHA-256 mismatch: expected=%s actual=%s",
                      m_expectedHash.c_str(), actual.c_str());
            remove(tmpPath.c_str());
            m_error = "Hash mismatch (file corrupted or tampered)";
            m_state = State::Error;
            return;
        }
        LOG_INFO("DownloadManager: SHA-256 OK");
    }

    // Atomic extract: extract into a staging dir under cache, then rename
    // into the active sdcafiine path. If extract fails, the active path is
    // never touched — no half-installed mod can be picked up by SDCafiine.
    m_state    = State::Extracting;
    m_progress = 0.0f;

    // Derive a staging path adjacent to tmpPath. tmpPath is "<cache>/<modid>.zip"
    // → stagingPath is "<cache>/staging/<modid>".
    std::string modId;
    std::string cacheRoot;
    {
        size_t slash = tmpPath.rfind('/');
        cacheRoot = (slash != std::string::npos) ? tmpPath.substr(0, slash) : ".";
        std::string base = (slash != std::string::npos) ? tmpPath.substr(slash + 1) : tmpPath;
        // Strip ".zip"
        size_t dot = base.rfind(".zip");
        modId = (dot != std::string::npos) ? base.substr(0, dot) : base;
    }
    std::string stagingPath = cacheRoot + "/staging/" + modId;

    // Pre-clean any leftover staging from a previous failed run. If the
    // cleanup fails (rmrf timeout, permission, etc.) we MUST abort -- a
    // partially-cleaned dir would mix old files with the new ZIP's contents
    // and silently corrupt the install.
    {
        struct stat st;
        if (stat(stagingPath.c_str(), &st) == 0) {
            if (!rmrf(stagingPath)) {
                m_error = "Cannot clean staging directory (previous install left junk)";
                m_state = State::Error;
                remove(tmpPath.c_str());
                return;
            }
        }
    }
    mkdirp(stagingPath);

    if (!ZipExtractor::extract(tmpPath, stagingPath)) {
        rmrf(stagingPath);
        remove(tmpPath.c_str());
        m_error = "Failed to extract ZIP";
        m_state = State::Error;
        return;
    }

    // Verify extract produced at least one file
    {
        int fileCount = 0;
        DIR* d = opendir(stagingPath.c_str());
        if (d) {
            struct dirent* e;
            while ((e = readdir(d)) != nullptr) {
                std::string n = e->d_name;
                if (n != "." && n != "..") { fileCount++; break; }
            }
            closedir(d);
        }
        if (fileCount == 0) {
            rmrf(stagingPath);
            remove(tmpPath.c_str());
            m_error = "Extract produced no files";
            m_state = State::Error;
            return;
        }
    }

    // Ensure parent of destDir exists (sdcafiine/<titleId>/)
    {
        size_t slash = destDir.rfind('/');
        if (slash != std::string::npos) mkdirp(destDir.substr(0, slash));
    }

    // Two-phase swap if destDir already exists (re-install / update)
    std::string oldPath = destDir + ".old";
    // Clear any stale .old from a prior aborted swap. If this fails, abort
    // -- otherwise the upcoming rename(destDir -> oldPath) will fail too.
    {
        struct stat st;
        if (stat(oldPath.c_str(), &st) == 0 && !rmrf(oldPath)) {
            m_error = "Cannot clear stale .old folder before swap";
            m_state = State::Error;
            rmrf(stagingPath);
            remove(tmpPath.c_str());
            return;
        }
    }
    struct stat st;
    bool destExists = (stat(destDir.c_str(), &st) == 0);
    if (destExists) {
        if (rename(destDir.c_str(), oldPath.c_str()) != 0) {
            LOG_ERROR("DownloadManager: cannot rename existing %s -> .old", destDir.c_str());
            rmrf(stagingPath);
            remove(tmpPath.c_str());
            m_error = "Cannot replace existing mod folder";
            m_state = State::Error;
            return;
        }
    }

    if (rename(stagingPath.c_str(), destDir.c_str()) != 0) {
        LOG_ERROR("DownloadManager: rename staging -> active failed (%s -> %s)",
                  stagingPath.c_str(), destDir.c_str());
        // Try to roll back .old if we created one
        if (destExists) rename(oldPath.c_str(), destDir.c_str());
        rmrf(stagingPath);
        remove(tmpPath.c_str());
        m_error = "Cannot move mod to active folder";
        m_state = State::Error;
        return;
    }

    // Successful swap — discard the old version and the zip
    if (destExists) rmrf(oldPath);
    remove(tmpPath.c_str());
    m_progress = 1.0f;
    m_state    = State::Done;
    LOG_INFO("DownloadManager: done -> %s", destDir.c_str());
}
