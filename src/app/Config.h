#pragma once
#include <string>
#include <vector>
#include "app/Paths.h"

struct Config {
    std::vector<std::string> repos;

    bool hasRepos() const { return !repos.empty(); }
    bool load();
    bool save();

    static std::string configPath() { return Paths::configFile(); }
};
