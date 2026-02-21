#include "Config.h"
#include "util/Logger.h"

#include <json.hpp>
#include <fstream>
#include <sys/stat.h>

bool Config::load() {
    std::ifstream f(CONFIG_PATH);
    if (!f.is_open()) {
        LOG_WARN("Config file not found: %s", CONFIG_PATH);
        return false;
    }

    try {
        nlohmann::json j;
        f >> j;

        repos.clear();
        if (j.contains("repos") && j["repos"].is_array()) {
            for (auto& r : j["repos"]) {
                if (r.is_string()) {
                    repos.push_back(r.get<std::string>());
                }
            }
        }

        LOG_INFO("Config loaded, %zu repo(s)", repos.size());
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Config parse error: %s", e.what());
        return false;
    }
}

bool Config::save() {
    // Ensure directory exists
    mkdir("sd:/wiiu/apps/modstore", 0755);

    nlohmann::json j;
    j["repos"] = repos;

    std::ofstream f(CONFIG_PATH);
    if (!f.is_open()) {
        LOG_ERROR("Could not write config: %s", CONFIG_PATH);
        return false;
    }

    f << j.dump(2);
    LOG_INFO("Config saved");
    return true;
}
