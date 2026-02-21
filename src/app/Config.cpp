#include "Config.h"
#include "util/Logger.h"

#include <json.hpp>
#include <fstream>
#include <sys/stat.h>

bool Config::load() {
    std::string path = configPath();
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
        LOG_INFO("Config loaded, %zu repo(s)", repos.size());
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Config parse error: %s", e.what());
        return false;
    }
}

bool Config::save() {
    std::string base = Paths::modstoreBase();
    mkdir(base.c_str(), 0755);

    nlohmann::json j;
    j["repos"] = repos;

    std::ofstream f(configPath());
    if (!f.is_open()) {
        LOG_ERROR("Cannot write config: %s", configPath().c_str());
        return false;
    }
    f << j.dump(2);
    return true;
}
