#include "RepoManager.h"
#include <atomic>
#include "util/Logger.h"

#include <curl/curl.h>
#include <json.hpp>
#include <sstream>
#include <algorithm>
#include <cctype>

// Same cap as HttpClient -- protect against runaway repo responses.
static constexpr size_t kRepoBodyMax = 10 * 1024 * 1024;

static size_t writeString(char* ptr, size_t size, size_t nmemb, std::string* s) {
    size_t add = size * nmemb;
    if (s->size() + add > kRepoBodyMax) return 0; // abort: body too large
    s->append(ptr, add);
    return add;
}

bool RepoManager::validateUrl(const std::string& url) {
    if (url.empty()) return false;
    if (url.substr(0, 7) != "http://" && url.substr(0, 8) != "https://") return false;
    size_t schemeEnd = url.find("://");
    if (schemeEnd == std::string::npos) return false;
    std::string rest = url.substr(schemeEnd + 3);
    if (rest.empty() || rest.find('.') == std::string::npos) return false;
    return true;
}

struct FetchCancelCtx { std::atomic<bool>* flag; };
static int fetchProgress(void* ud, curl_off_t, curl_off_t, curl_off_t, curl_off_t) {
    auto* ctx = static_cast<FetchCancelCtx*>(ud);
    return (ctx && ctx->flag && ctx->flag->load()) ? 1 : 0;
}
static std::string fetchUrl(const std::string& url, std::string& error, std::atomic<bool>* cancelFlag = nullptr) {
    LOG_INFO("fetchUrl: %s", url.c_str());
    std::string body;
    CURL* curl = curl_easy_init();
    if (!curl) { error = "curl_easy_init failed"; return ""; }
    curl_easy_setopt(curl, CURLOPT_URL,            url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,  writeString);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA,      &body);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT,        5L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 3L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL,       1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT,      "WiiUModStore/0.1");
    FetchCancelCtx ctx{ cancelFlag };
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, fetchProgress);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA,     &ctx);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS,       0L);
    LOG_INFO("calling curl_easy_perform...");
    CURLcode res = curl_easy_perform(curl);
    LOG_INFO("curl returned: %d", res);
    long httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    curl_easy_cleanup(curl);
    if (res != CURLE_OK) { error = curl_easy_strerror(res); LOG_ERROR("curl error: %s", error.c_str()); return ""; }
    if (httpCode >= 400) { error = "HTTP " + std::to_string(httpCode); return ""; }
    if (body.empty())    { error = "Empty response"; return ""; }
    LOG_INFO("fetchUrl OK: %zu bytes", body.size());
    return body;
}

static std::string resolveUrl(const std::string& base, const std::string& path) {
    if (path.substr(0, 4) == "http") return path;
    size_t slash = base.rfind('/');
    if (slash == std::string::npos) return path;
    return base.substr(0, slash + 1) + path;
}

void RepoManager::fetch(const std::string& url, std::atomic<bool>* cancelFlag) {
    m_lastError.clear();
    m_repo = {};

    if (!validateUrl(url)) {
        m_lastError = "Invalid URL: must start with http:// or https://";
        LOG_ERROR("RepoManager: %s", m_lastError.c_str());
        return;
    }

    std::string err;
    std::string body = fetchUrl(url, err, cancelFlag);
    if (body.empty()) {
        m_lastError = "Network error: " + err;
        LOG_ERROR("RepoManager: %s", m_lastError.c_str());
        return;
    }

    try {
        auto j = nlohmann::json::parse(body);
        m_repo.name = j.value("name", "Unknown Repo");

        int formatVersion = j.value("formatVersion", 1);
        if (formatVersion < REPO_FORMAT_MIN) {
            LOG_WARN("RepoManager: repo format %d is older than min supported (%d), continuing anyway",
                     formatVersion, REPO_FORMAT_MIN);
        } else if (formatVersion > REPO_FORMAT_MAX) {
            m_lastError = "Repo requires a newer version of this app (format v"
                        + std::to_string(formatVersion) + ", app supports up to v"
                        + std::to_string(REPO_FORMAT_MAX) + ")";
            LOG_ERROR("RepoManager: %s", m_lastError.c_str());
            return;
        }

        if (!j.contains("games") || !j["games"].is_array()) {
            m_lastError = "Invalid repo format: missing games array";
            return;
        }

        int totalGames    = 0;
        int fetchFailures = 0;
        std::string firstFetchError;

        for (auto& jg : j["games"]) {
            totalGames++;
            if (jg.contains("path")) {
                if (!jg["path"].is_string()) {
                    LOG_WARN("RepoManager: skipping game with non-string path field");
                    fetchFailures++;
                    continue;
                }
                std::string gamePath = jg["path"].get<std::string>();
                std::string gameUrl  = resolveUrl(url, gamePath);
                LOG_INFO("RepoManager: fetching game from %s", gameUrl.c_str());

                std::string gameBody = fetchUrl(gameUrl, err, cancelFlag);
                if (gameBody.empty()) {
                    LOG_WARN("RepoManager: failed to fetch %s: %s", gameUrl.c_str(), err.c_str());
                    fetchFailures++;
                    if (firstFetchError.empty()) firstFetchError = err;
                    continue;
                }
                parseGame(gameBody);
            } else {
                parseGame(jg.dump());
            }
        }

        if (m_repo.games.empty()) {
            if (totalGames == 0) {
                m_lastError = "Repo lists no games";
            } else if (fetchFailures == totalGames) {
                m_lastError = "All " + std::to_string(totalGames)
                            + " game fetches failed: " + firstFetchError;
            } else {
                m_lastError = "Repo contains no valid mods";
            }
        }
    } catch (const nlohmann::json::exception& e) {
        m_lastError = std::string("JSON error: ") + e.what();
        LOG_ERROR("RepoManager: %s", m_lastError.c_str());
        return;
    }
}

std::optional<Game> RepoManager::parseGameFromJson(const std::string& json) {
    try {
        auto jg = nlohmann::json::parse(json);
        if (!jg.contains("name") || (!jg.contains("titleIds") && !jg.contains("title_ids")) || !jg.contains("mods")) {
            LOG_WARN("RepoManager: skipping game - missing required fields");
            return std::nullopt;
        }
        Game game;
        game.name = jg["name"].get<std::string>();
        game.icon = jg.value("icon", "");
        
        // titleIds validation
        auto tids = jg.contains("titleIds") ? jg["titleIds"] : jg["title_ids"];
        if (!tids.is_array() || tids.empty()) {
            LOG_WARN("RepoManager: skipping game - titleIds must be non-empty array");
            return std::nullopt;
        }
        for (auto& tid : tids) game.titleIds.push_back(tid.get<std::string>());
        
        // mods validation
        if (!jg["mods"].is_array() || jg["mods"].empty()) {
            LOG_WARN("RepoManager: skipping game - mods must be non-empty array");
            return std::nullopt;
        }
        
        for (auto& jm : jg["mods"]) {
            if (!jm.contains("id") || !jm.contains("name") ||
                !jm.contains("version") || !jm.contains("download")) {
                LOG_WARN("RepoManager: skipping mod - missing required fields");
                continue;
            }
            Mod mod;
            mod.id = jm["id"].get<std::string>();
            bool validId = !mod.id.empty() && std::all_of(mod.id.begin(), mod.id.end(),
                [](char c){ return isalnum(c) || c == '-' || c == '_'; });
            if (!validId) { LOG_WARN("RepoManager: skipping mod with invalid id: %s", mod.id.c_str()); continue; }
            mod.name        = jm["name"].get<std::string>();
            mod.version     = jm["version"].get<std::string>();
            mod.download    = jm["download"].get<std::string>();
            mod.author      = jm.value("author", "Unknown");
            mod.description = jm.value("description", "");
            mod.type        = jm.value("type", "mod");
            mod.thumbnail   = jm.value("thumbnail", "");
            if (jm.contains("includes") && jm["includes"].is_array())
                for (auto& s : jm["includes"]) mod.includes.push_back(s.get<std::string>());
            if (jm.contains("screenshots") && jm["screenshots"].is_array())
                for (auto& s : jm["screenshots"]) mod.screenshots.push_back(s.get<std::string>());
            mod.releaseDate = jm.value("releaseDate", "");
            mod.changelog   = jm.value("changelog", "");
            mod.license     = jm.value("license", "");
            
            // fileSize with type check and bounds validation
            if (jm.contains("fileSize") && jm["fileSize"].is_number()) {
                int64_t size = jm["fileSize"].get<int64_t>();
                mod.fileSize = (size > 0) ? static_cast<uint64_t>(size) : 0;
            }
            
            if (jm.contains("requirements") && jm["requirements"].is_array())
                for (auto& s : jm["requirements"]) mod.requirements.push_back(s.get<std::string>());
            if (jm.contains("tags") && jm["tags"].is_array())
                for (auto& s : jm["tags"]) mod.tags.push_back(s.get<std::string>());
            if (!RepoManager::validateUrl(mod.download)) {
                LOG_WARN("RepoManager: skipping mod '%s' - invalid download URL", mod.id.c_str());
                continue;
            }
            game.mods.push_back(std::move(mod));
        }
        if (game.mods.empty()) return std::nullopt;
        return game;
    } catch (const nlohmann::json::exception& e) {
        LOG_WARN("RepoManager: JSON parse error: %s", e.what());
        return std::nullopt;
    }
}


void RepoManager::parseGame(const std::string& json) {
    try {
        auto game = parseGameFromJson(json);
        if (game) m_repo.games.push_back(std::move(*game));
    } catch (const nlohmann::json::exception& e) {
        LOG_WARN("RepoManager: parseGame failed: %s", e.what());
    }
}
