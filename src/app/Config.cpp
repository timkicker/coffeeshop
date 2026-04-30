#include "Config.h"
#include "util/Logger.h"

#include <json.hpp>
#include <fstream>
#include <sys/stat.h>
#include <cerrno>
#include <cstring>

bool Config::loadFrom(const std::string& path) {
    LOG_INFO("Loading config from: %s", path.c_str());
    std::ifstream f(path);
    if (!f.is_open()) {
        LOG_WARN("Config not found: %s", path.c_str());
        return false;
    }

    try {
        nlohmann::json j;
        f >> j;
        repos.clear();
        if (j.contains("repos") && j["repos"].is_array())
            for (auto& r : j["repos"])
                if (r.is_string()) repos.push_back(r.get<std::string>());
        musicTrack = j.value("musicTrack", "off");
        LOG_INFO("Config loaded, %zu repo(s)", repos.size());
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Config parse error: %s", e.what());
        return false;
    }
}

bool Config::saveTo(const std::string& path) {
    nlohmann::json j;
    j["repos"] = repos;
    j["musicTrack"] = musicTrack;

    std::ofstream f(path);
    if (!f.is_open()) {
        LOG_ERROR("Cannot write config: %s", path.c_str());
        return false;
    }
    f << j.dump(2);
    return true;
}

bool Config::load() {
    return loadFrom(configPath());
}

bool Config::save() {
    std::string base = Paths::modstoreBase();
    if (mkdir(base.c_str(), 0755) != 0 && errno != EEXIST) {
        LOG_WARN("Config: mkdir(%s) failed: %s", base.c_str(), strerror(errno));
    }
    return saveTo(configPath());
}
