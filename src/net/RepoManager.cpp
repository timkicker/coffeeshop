#include "RepoManager.h"
#include "HttpClient.h"
#include "util/Logger.h"

#include <json.hpp>

// Resolve a relative path against a base URL
static std::string resolveUrl(const std::string& base, const std::string& relative) {
    // Find last slash in base URL
    size_t lastSlash = base.rfind('/');
    if (lastSlash == std::string::npos) return relative;
    return base.substr(0, lastSlash + 1) + relative;
}

bool RepoManager::fetch(const std::string& repoUrl) {
    m_loaded = false;
    m_repo   = {};

    LOG_INFO("Fetching repo: %s", repoUrl.c_str());

    std::string body;
    if (!HttpClient::get(repoUrl, body)) {
        m_lastError = "Failed to fetch repo.json";
        return false;
    }

    try {
        auto j = nlohmann::json::parse(body);

        m_repo.name        = j.value("name", "");
        m_repo.description = j.value("description", "");
        m_repo.author      = j.value("author", "");

        if (!j.contains("games") || !j["games"].is_array()) {
            m_lastError = "repo.json has no games array";
            return false;
        }

        for (auto& g : j["games"]) {
            std::string gamePath = g.value("path", "");
            if (gamePath.empty()) continue;

            Game game;
            game.id = g.value("id", "");

            std::string gameUrl = resolveUrl(repoUrl, gamePath);
            if (!fetchGame(repoUrl, gameUrl, game)) {
                LOG_WARN("Failed to fetch game: %s", gameUrl.c_str());
                continue;
            }

            m_repo.games.push_back(std::move(game));
        }

        LOG_INFO("Repo loaded: %s (%zu games)", m_repo.name.c_str(), m_repo.games.size());
        m_loaded = true;
        return true;

    } catch (const std::exception& e) {
        m_lastError = std::string("JSON parse error: ") + e.what();
        LOG_ERROR("%s", m_lastError.c_str());
        return false;
    }
}

bool RepoManager::fetchGame(const std::string& baseUrl, const std::string& gameUrl, Game& game) {
    std::string body;
    if (!HttpClient::get(gameUrl, body)) return false;

    try {
        auto j = nlohmann::json::parse(body);

        game.name  = j.value("name", "");
        game.cover = j.value("cover", "");

        if (j.contains("title_ids") && j["title_ids"].is_array())
            for (auto& id : j["title_ids"])
                if (id.is_string()) game.titleIds.push_back(id);

        if (j.contains("mods") && j["mods"].is_array()) {
            for (auto& m : j["mods"]) {
                Mod mod;
                mod.id          = m.value("id", "");
                mod.name        = m.value("name", "");
                mod.type        = m.value("type", "mod");
                mod.version     = m.value("version", "");
                mod.author      = m.value("author", "");
                mod.description = m.value("description", "");
                mod.thumbnail   = m.value("thumbnail", "");
                mod.download    = m.value("download", "");
                mod.modpackPath = m.value("modpack_path", "content/");

                if (m.contains("screenshots") && m["screenshots"].is_array())
                    for (auto& s : m["screenshots"])
                        if (s.is_string()) mod.screenshots.push_back(s);

                if (m.contains("includes") && m["includes"].is_array())
                    for (auto& inc : m["includes"])
                        if (inc.is_string()) mod.includes.push_back(inc);

                game.mods.push_back(std::move(mod));
            }
        }

        LOG_INFO("Game loaded: %s (%zu mods)", game.name.c_str(), game.mods.size());
        return true;

    } catch (const std::exception& e) {
        LOG_ERROR("Game JSON parse error: %s", e.what());
        return false;
    }
}
