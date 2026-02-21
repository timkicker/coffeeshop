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

static std::vector<std::string> listDirs(const std::string& path) {
    std::vector<std::string> result;
    DIR* d = opendir(path.c_str());
    if (!d) return result;
    struct dirent* e;
    while ((e = readdir(d)) != nullptr) {
        std::string name = e->d_name;
        if (name == "." || name == "..") continue;
        if (isDir(path + "/" + name))
            result.push_back(name);
    }
    closedir(d);
    return result;
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
        else remove(child.c_str());
    }
    closedir(d);
    return rmdir(path.c_str()) == 0;
}

std::vector<InstalledMod> InstalledScanner::scan() {
    std::vector<InstalledMod> result;
    std::string base = Paths::sdcafiineBase();

    for (auto& titleId : listDirs(base)) {
        std::string titlePath = base + "/" + titleId;
        for (auto& folderName : listDirs(titlePath)) {
            InstalledMod mod;
            mod.titleId = titleId;
            mod.active  = (folderName.substr(0, 10) != ".disabled_");
            mod.id      = mod.active ? folderName : folderName.substr(10);
            mod.path    = titlePath + "/" + folderName;
            mod.name    = mod.id; // fallback

            // Try reading modinfo.json
            std::string infoPath = mod.path + "/modinfo.json";
            std::ifstream f(infoPath);
            if (f.is_open()) {
                try {
                    nlohmann::json j;
                    f >> j;
                    if (j.contains("version")) mod.version = j["version"];
                    if (j.contains("id"))      mod.name    = j["id"]; // use id as name fallback
                } catch (...) {}
            }

            result.push_back(mod);
        }
    }
    return result;
}

bool InstalledScanner::setActive(InstalledMod& mod, bool active) {
    std::string dir    = mod.path.substr(0, mod.path.rfind('/') + 1);
    std::string newName = active ? mod.id : ".disabled_" + mod.id;
    std::string newPath = dir + newName;

    if (rename(mod.path.c_str(), newPath.c_str()) != 0) {
        LOG_ERROR("InstalledScanner: rename failed: %s -> %s", mod.path.c_str(), newPath.c_str());
        return false;
    }
    mod.active = active;
    mod.path   = newPath;
    return true;
}

bool InstalledScanner::remove(const InstalledMod& mod) {
    return rmrf(mod.path);
}

bool InstalledScanner::hasUpdate(const InstalledMod& mod) {
    return !mod.repoVersion.empty() && mod.repoVersion != mod.version;
}
