#pragma once

#include <string>
#include <optional>
#include <vector>
#include <cstdint>

struct Mod {
    std::string id;
    std::string name;
    std::string author;
    std::string version;
    std::string description;
    std::string download;
    std::string type;      // "mod" or "modpack"
    std::string thumbnail; // URL
    std::vector<std::string> includes;
    std::vector<std::string> screenshots;
    // Optional metadata
    std::string              releaseDate;   // e.g. "2024-03-15"
    std::string              changelog;     // free text
    std::string              license;       // e.g. "CC BY-NC"
    std::vector<std::string> requirements;  // e.g. ["60fps patch"]
    std::vector<std::string> tags;          // e.g. ["course", "skin"]
    uint64_t                 fileSize = 0;  // bytes, 0 = unknown
};

struct Game {
    std::string name;
    std::string icon;  // optional URL
    std::vector<std::string> titleIds;
    std::vector<Mod> mods;
};

struct Repo {
    std::string name;
    std::vector<Game> games;
};

// Repo format versions this app understands
static constexpr int REPO_FORMAT_MIN = 1;
static constexpr int REPO_FORMAT_MAX = 1;

class RepoManager {
public:
    // Returns false if URL is invalid format
    static bool validateUrl(const std::string& url);

    void fetch(const std::string& url);

    const Repo&        repo()      const { return m_repo; }
    const std::string& lastError() const { return m_lastError; }

    // Testable: parse a single game JSON object into a Game struct
    // Returns empty optional if required fields are missing or all mods are invalid
    static std::optional<Game> parseGameFromJson(const std::string& json);

private:
    void parseGame(const std::string& json);

    Repo        m_repo;
    std::string m_lastError;
};
