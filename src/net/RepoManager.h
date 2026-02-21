#pragma once

#include <string>
#include <vector>

struct Mod {
    std::string id;
    std::string name;
    std::string author;
    std::string version;
    std::string description;
    std::string download;
    std::string type; // "mod" or "modpack"
    std::vector<std::string> includes;
    std::vector<std::string> screenshots;
};

struct Game {
    std::string name;
    std::vector<std::string> titleIds;
    std::vector<Mod> mods;
};

struct Repo {
    std::string name;
    std::vector<Game> games;
};

class RepoManager {
public:
    // Returns false if URL is invalid format
    static bool validateUrl(const std::string& url);

    void fetch(const std::string& url);

    const Repo&        repo()      const { return m_repo; }
    const std::string& lastError() const { return m_lastError; }

private:
    void parseGame(const std::string& json);

    Repo        m_repo;
    std::string m_lastError;
};
