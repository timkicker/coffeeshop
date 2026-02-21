#pragma once
#include <string>
#include <vector>

struct Mod {
    std::string id;
    std::string name;
    std::string type;        // "mod" or "modpack"
    std::string version;
    std::string author;
    std::string description;
    std::string thumbnail;
    std::vector<std::string> screenshots;
    std::string download;
    std::string modpackPath;
    std::vector<std::string> includes; // for modpacks
};

struct Game {
    std::string id;
    std::string name;
    std::string cover;
    std::vector<std::string> titleIds;
    std::vector<Mod> mods;
};

struct Repo {
    std::string name;
    std::string description;
    std::string author;
    std::vector<Game> games;
};

class RepoManager {
public:
    // Fetch and parse a repo from a URL.
    // baseUrl is the URL of repo.json, used to resolve relative game.json paths.
    bool fetch(const std::string& repoUrl);

    const Repo& repo() const { return m_repo; }
    bool isLoaded() const { return m_loaded; }
    const std::string& lastError() const { return m_lastError; }

private:
    bool fetchGame(const std::string& baseUrl, const std::string& gamePath, Game& game);

    Repo        m_repo;
    bool        m_loaded   = false;
    std::string m_lastError;
};
