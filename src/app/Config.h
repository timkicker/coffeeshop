#pragma once
#include <string>
#include <vector>
#include "app/Paths.h"

struct Config {
    std::vector<std::string> repos;
    std::string musicTrack = "off";

    // UI preferences -- persisted across launches.
    std::string sortMode      = "default";  // "default", "name", "version"
    std::vector<std::string> activeTags;    // last selected browse-tag filter
    std::string lastTab       = "browse";   // "browse", "installed", "settings"

    bool hasRepos() const { return !repos.empty(); }
    bool load();
    bool save();

    // Path-parameterized overloads (used by tests, but also available in prod)
    bool loadFrom(const std::string& path);
    bool saveTo(const std::string& path);

    static std::string configPath() { return Paths::configFile(); }
};
