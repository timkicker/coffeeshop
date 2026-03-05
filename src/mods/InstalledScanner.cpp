#include <unistd.h>
#include "InstalledScanner.h"
#include "app/Paths.h"
#include "util/Logger.h"
#include <json.hpp>
#include <dirent.h>
#include <sys/stat.h>
#include <fstream>
#include <cstring>

static bool isDir(const std::string& path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

static void mkdirp(const std::string& path) {
    for (size_t i = 1; i <= path.size(); i++) {
        if (i == path.size() || path[i] == '/')
            mkdir(path.substr(0, i).c_str(), 0755);
    }
}

static bool rmrf(const std::string& path) {
    DIR* d = opendir(path.c_str());
    if (!d) { remove(path.c_str()); return true; }
    struct dirent* e;
    while ((e = readdir(d)) != nullptr) {
        std::string name = e->d_name;
        if (name == "." || name == "..") continue;
        std::string child = path + "/" + name;
        if (isDir(child)) rmrf(child);
        else ::remove(child.c_str());
    }
    closedir(d);
    return rmdir(path.c_str()) == 0;
}

static std::vector<std::string> listDirs(const std::string& path) {
    LOG_INFO("listDirs: %s", path.c_str());
    std::vector<std::string> result;
    DIR* d = opendir(path.c_str());
    if (!d) {
        LOG_WARN("listDirs: opendir failed for %s", path.c_str());
        return result;
    }
    struct dirent* e;
    while ((e = readdir(d)) != nullptr) {
        std::string name = e->d_name;
        if (name == "." || name == "..") continue;
        if (isDir(path + "/" + name))
            result.push_back(name);
    }
    closedir(d);
    LOG_INFO("listDirs: found %zu dirs", result.size());
    return result;
}

static InstalledMod readMod(const std::string& basePath,
                             const std::string& titleId,
                             const std::string& modId,
                             bool active) {
    InstalledMod mod;
    mod.titleId = titleId;
    mod.id      = modId;
    mod.name    = modId;
    mod.active  = active;
    mod.path    = basePath + "/" + titleId + "/" + modId;

    std::ifstream f(mod.path + "/modinfo.json");
    if (f.is_open()) {
        try {
            nlohmann::json j;
            f >> j;
            if (j.contains("version")) mod.version = j["version"].get<std::string>();
            if (j.contains("id"))      mod.name    = j["id"].get<std::string>();
        } catch (...) {}
    }
    return mod;
}

std::vector<InstalledMod> InstalledScanner::scan() {
    LOG_INFO("InstalledScanner::scan() start");
    std::vector<InstalledMod> result;
    
    std::string activeBase = Paths::sdcafiineBase();
    LOG_INFO("Scanning active mods in: %s", activeBase.c_str());
    for (auto& titleId : listDirs(activeBase)) {
        LOG_INFO("Active titleId: %s", titleId.c_str());
        for (auto& modId : listDirs(activeBase + "/" + titleId)) {
            LOG_INFO("Active mod: %s/%s", titleId.c_str(), modId.c_str());
            result.push_back(readMod(activeBase, titleId, modId, true));
        }
    }

    std::string disabledBase = Paths::disabledBase();
    LOG_INFO("Scanning disabled mods in: %s", disabledBase.c_str());
    for (auto& titleId : listDirs(disabledBase)) {
        LOG_INFO("Disabled titleId: %s", titleId.c_str());
        for (auto& modId : listDirs(disabledBase + "/" + titleId)) {
            LOG_INFO("Disabled mod: %s/%s", titleId.c_str(), modId.c_str());
            result.push_back(readMod(disabledBase, titleId, modId, false));
        }
    }
    
    LOG_INFO("InstalledScanner::scan() done: %zu mods", result.size());
    return result;
}

bool InstalledScanner::setActive(InstalledMod& mod, bool active) {
    std::string srcBase  = active ? Paths::disabledBase() : Paths::sdcafiineBase();
    std::string dstBase  = active ? Paths::sdcafiineBase() : Paths::disabledBase();
    std::string src      = srcBase  + "/" + mod.titleId + "/" + mod.id;
    std::string dstDir   = dstBase  + "/" + mod.titleId;
    std::string dst      = dstDir   + "/" + mod.id;

    mkdirp(dstDir);

    if (rename(src.c_str(), dst.c_str()) != 0) {
        LOG_ERROR("InstalledScanner: rename failed: %s -> %s", src.c_str(), dst.c_str());
        return false;
    }

    mod.active = active;
    mod.path   = dst;
    LOG_INFO("InstalledScanner: %s -> %s", mod.id.c_str(), active ? "active" : "disabled");
    return true;
}

bool InstalledScanner::remove(const InstalledMod& mod) {
    return rmrf(mod.path);
}

bool InstalledScanner::hasUpdate(const InstalledMod& mod) {
    return !mod.repoVersion.empty() && mod.repoVersion != mod.version;
}
