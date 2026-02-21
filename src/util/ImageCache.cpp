#include "ImageCache.h"
#include "app/Paths.h"
#include "util/Logger.h"

#include <curl/curl.h>
#include <sys/stat.h>
#include <fstream>
#include <algorithm>

static size_t writeVec(char* ptr, size_t size, size_t nmemb, std::vector<uint8_t>* v) {
    v->insert(v->end(), (uint8_t*)ptr, (uint8_t*)ptr + size * nmemb);
    return size * nmemb;
}

static void mkdirp(const std::string& path) {
    for (size_t i = 1; i <= path.size(); i++)
        if (i == path.size() || path[i] == '/')
            mkdir(path.substr(0, i).c_str(), 0755);
}

// Cache file path for a URL (simple hash)
static std::string cacheFile(const std::string& url) {
    size_t h = std::hash<std::string>{}(url);
    char buf[64];
    snprintf(buf, sizeof(buf), "%016zx", h);
    return Paths::cacheDir() + "/images/" + std::string(buf) + ".img";
}

void ImageCache::request(const std::string& url) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_cache.find(url);
        if (it != m_cache.end()) return; // already queued or done
        m_cache[url].state = State::Loading;
    }

    // Fire and forget thread
    std::thread([this, url]() { download(url); }).detach();
}

void ImageCache::download(const std::string& url) {
    // Check disk cache first
    std::string file = cacheFile(url);
    {
        std::ifstream f(file, std::ios::binary);
        if (f.is_open()) {
            std::vector<uint8_t> data(
                (std::istreambuf_iterator<char>(f)),
                std::istreambuf_iterator<char>());
            if (!data.empty()) {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_cache[url].data  = std::move(data);
                m_cache[url].state = State::Ready;
                LOG_INFO("ImageCache: loaded from disk: %s", url.c_str());
                return;
            }
        }
    }

    // Download
    std::vector<uint8_t> data;
    CURL* curl = curl_easy_init();
    if (!curl) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_cache[url].state = State::Failed;
        return;
    }

    curl_easy_setopt(curl, CURLOPT_URL,            url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,  writeVec);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA,      &data);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT,        30L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 512L);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME,  15L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER,  0L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT,      "WiiUModStore/0.1");

    CURLcode res = curl_easy_perform(curl);
    long httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK || httpCode >= 400 || data.empty()) {
        LOG_WARN("ImageCache: download failed for %s (curl=%d http=%ld)",
                 url.c_str(), res, httpCode);
        std::lock_guard<std::mutex> lock(m_mutex);
        m_cache[url].state = State::Failed;
        return;
    }

    // Save to disk cache
    mkdirp(Paths::cacheDir() + "/images");
    std::ofstream f(file, std::ios::binary);
    if (f.is_open()) f.write((char*)data.data(), data.size());

    std::lock_guard<std::mutex> lock(m_mutex);
    m_cache[url].data  = std::move(data);
    m_cache[url].state = State::Ready;
    LOG_INFO("ImageCache: downloaded: %s", url.c_str());
}

SDL_Texture* ImageCache::texture(const std::string& url, SDL_Renderer* renderer) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_cache.find(url);
    if (it == m_cache.end()) return nullptr;

    Entry& e = it->second;
    if (e.state != State::Ready) return nullptr;

    // Already have texture
    if (e.tex) return e.tex;

    // Create texture from raw bytes (main thread only)
    SDL_RWops* rw = SDL_RWFromMem(e.data.data(), (int)e.data.size());
    if (!rw) { e.state = State::Failed; return nullptr; }

    SDL_Surface* surf = IMG_Load_RW(rw, 1); // 1 = auto-close rw
    if (!surf) {
        LOG_WARN("ImageCache: IMG_Load_RW failed for %s: %s", url.c_str(), IMG_GetError());
        e.state = State::Failed;
        return nullptr;
    }

    e.tex = SDL_CreateTextureFromSurface(renderer, surf);
    SDL_FreeSurface(surf);

    if (!e.tex) {
        LOG_WARN("ImageCache: CreateTexture failed: %s", SDL_GetError());
        e.state = State::Failed;
    }

    return e.tex;
}

void ImageCache::clear(SDL_Renderer* renderer) {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& pair : m_cache) {
        if (pair.second.tex)
            SDL_DestroyTexture(pair.second.tex);
    }
    m_cache.clear();
}
