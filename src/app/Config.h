#pragma once
#include <string>
#include <vector>

struct Config {
    std::vector<std::string> repos;

    bool hasRepos() const { return !repos.empty(); }

    // Load from SD card, returns false if file doesn't exist or is invalid
    bool load();

    // Save current config back to SD card
    bool save();

    static constexpr const char* CONFIG_PATH = "/vol/content/config.json";
};
