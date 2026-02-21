#include "RepoManager.h"
#include "util/Logger.h"

#include <curl/curl.h>
#include <json.hpp>
#include <sstream>
#include <algorithm>

static size_t writeString(char* ptr, size_t size, size_t nmemb, std::string* s) {
    s->append(ptr, size * nmemb);
    return size * nmemb;
}

bool RepoManager::validateUrl(const std::string& url) {
    if (url.empty()) return false;
    // Must start with http:// or https://
    if (url.substr(0, 7) != "http://" && url.substr(0, 8) != "https://") return false;
    // Must have something after the scheme
    size_t schemeEnd = url.find("://");
    if (schemeEnd == std::string::npos) return false;
    std::string rest = url.substr(schemeEnd + 3);
    if (rest.empty() || rest.find('.') == std::string::npos) return false;
    return true;
}

static std::string fetchUrl(const std::string& url, std::string& error) {
    std::string body;
    CURL* curl = curl_easy_init();
    if (!curl) { error = "curl_easy_init failed"; return ""; }

    curl_easy_setopt(curl, CURLOPT_URL,            url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,  writeString);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA,      &body);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT,        30L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT,      "WiiUModStore/0.1");

    CURLcode res = curl_easy_perform(curl);
    long httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) { error = curl_easy_strerror(res); return ""; }
    if (httpCode >= 400) { error = "HTTP " + std::to_string(httpCode); return ""; }
    if (body.empty())    { error = "Empty response"; return ""; }
    return body;
}

// Resolve a relative path against a base URL
// e.g. base = "https://host/repo.json", path = "games/mk8/game.json"
// -> "https://host/games/mk8/game.json"
static std::string resolveUrl(const std::string& base, const std::string& path) {
    if (path.substr(0, 4) == "http") return path; // already absolute
    size_t slash = base.rfind('/');
    if (slash == std::string::npos) return path;
    return base.substr(0, slash + 1) + path;
}

void RepoManager::fetch(const std::string& url) {
    m_lastError.clear();
    m_repo = {};

    if (!validateUrl(url)) {
        m_lastError = "Invalid URL: must start with http:// or https://";
        LOG_ERROR("RepoManager: %s", m_lastError.c_str());
        return;
    }

    std::string err;
    std::string body = fetchUrl(url, err);
    if (body.empty()) {
        m_lastError = "Network error: " + err;
        LOG_ERROR("RepoManager: %s", m_lastError.c_str());
        return;
    }

    // Parse index
    try {
        auto j = nlohmann::json::parse(body);
        m_repo.name = j.value("name", "Unknown Repo");

        // Format version check
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

        for (auto& jg : j["games"]) {
            if (jg.contains("path")) {
                // Indirect reference - fetch the game.json
                std::string gamePath = jg["path"].get<std::string>();
                std::string gameUrl  = resolveUrl(url, gamePath);
                LOG_INFO("RepoManager: fetching game from %s", gameUrl.c_str());

                std::string gameBody = fetchUrl(gameUrl, err);
                if (gameBody.empty()) {
                    LOG_WARN("RepoManager: failed to fetch %s: %s", gameUrl.c_str(), err.c_str());
                    continue;
                }
                parseGame(gameBody);
            } else {
                // Inline game object
                parseGame(jg.dump());
            }
        }
    } catch (const nlohmann::json::exception& e) {
        m_lastError = std::string("JSON error: ") + e.what();
        LOG_ERROR("RepoManager: %s", m_lastError.c_str());
        return;
    }

    if (m_repo.games.empty())
        m_lastError = "Repo contains no valid mods";
}

void RepoManager::parseGame(const std::string& json) {
    try {
        auto jg = nlohmann::json::parse(json);

        if (!jg.contains("name") || (!jg.contains("titleIds") && !jg.contains("title_ids")) || !jg.contains("mods")) {
            LOG_WARN("RepoManager: skipping game - missing required fields");
            return;
        }

        Game game;
        game.name = jg["name"].get<std::string>();
        auto tids = jg.contains("titleIds") ? jg["titleIds"] : jg["title_ids"];
        for (auto& tid : tids) game.titleIds.push_back(tid.get<std::string>());

        for (auto& jm : jg["mods"]) {
            if (!jm.contains("id") || !jm.contains("name") ||
                !jm.contains("version") || !jm.contains("download")) {
                LOG_WARN("RepoManager: skipping mod - missing required fields");
                continue;
            }
            Mod mod;
            mod.id          = jm["id"].get<std::string>();
            mod.name        = jm["name"].get<std::string>();
            mod.version     = jm["version"].get<std::string>();
            mod.download    = jm["download"].get<std::string>();
            mod.author      = jm.value("author", "Unknown");
            mod.description = jm.value("description", "");
            mod.type        = jm.value("type", "mod");

            if (jm.contains("includes") && jm["includes"].is_array())
                for (auto& s : jm["includes"]) mod.includes.push_back(s.get<std::string>());
            if (jm.contains("screenshots") && jm["screenshots"].is_array())
                for (auto& s : jm["screenshots"]) mod.screenshots.push_back(s.get<std::string>());

            if (!RepoManager::validateUrl(mod.download)) {
                LOG_WARN("RepoManager: skipping mod '%s' - invalid download URL", mod.id.c_str());
                continue;
            }
            game.mods.push_back(std::move(mod));
        }

        if (!game.mods.empty())
            m_repo.games.push_back(std::move(game));

    } catch (const nlohmann::json::exception& e) {
        LOG_WARN("RepoManager: parseGame failed: %s", e.what());
    }
}
