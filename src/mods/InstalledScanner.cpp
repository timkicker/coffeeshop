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

// Iterate "/"-delimited boundaries instead of running mkdir at every char index.
// Saves O(path-length) syscalls on slow SD-card filesystems.
static void mkdirp(const std::string& path) {
    size_t pos = 1;
    while (pos < path.size()) {
        size_t slash = path.find('/', pos);
        if (slash == std::string::npos) break;
        mkdir(path.substr(0, slash).c_str(), 0755);
        pos = slash + 1;
    }
    mkdir(path.c_str(), 0755);
}

// Recursive delete with symlink-loop protection.
// - Uses lstat (not stat) so symlinks are detected before recursion.
// - Tracks visited (dev,inode) pairs so a directory hardlinked or
//   symlinked-into-itself can't trap us in infinite recursion.
// - Caps recursion depth at 64 -- Wii U mods don't go that deep, anything
//   beyond is malicious or corrupt.
#include <set>
struct VisitedKey { dev_t dev; ino_t ino; };
static bool operator<(const VisitedKey& a, const VisitedKey& b) {
    if (a.dev != b.dev) return a.dev < b.dev;
    return a.ino < b.ino;
}

static bool rmrfImpl(const std::string& path, std::set<VisitedKey>& visited, int depth) {
    if (depth > 64) {
        LOG_WARN("rmrf: depth limit hit at %s -- aborting", path.c_str());
        return false;
    }
    struct stat st;
    if (lstat(path.c_str(), &st) != 0) return true; // already gone
    if (!S_ISDIR(st.st_mode)) {
        // Symlink or regular file -- remove without following.
        return remove(path.c_str()) == 0;
    }
    VisitedKey key{st.st_dev, st.st_ino};
    if (!visited.insert(key).second) {
        LOG_WARN("rmrf: cycle detected at %s -- aborting", path.c_str());
        return false;
    }

    DIR* d = opendir(path.c_str());
    if (!d) return false;
    bool ok = true;
    struct dirent* e;
    while ((e = readdir(d)) != nullptr) {
        std::string name = e->d_name;
        if (name == "." || name == "..") continue;
        if (!rmrfImpl(path + "/" + name, visited, depth + 1)) ok = false;
    }
    closedir(d);
    if (rmdir(path.c_str()) != 0) ok = false;
    return ok;
}

static bool rmrf(const std::string& path) {
    std::set<VisitedKey> visited;
    return rmrfImpl(path, visited, 0);
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

    // Some platforms refuse rename() if dst exists. If a stale folder is in
    // the way (e.g. previous failed activation), remove it first.
    struct stat st;
    if (stat(dst.c_str(), &st) == 0) {
        LOG_WARN("InstalledScanner: rename target exists, removing first: %s", dst.c_str());
        if (!rmrf(dst)) {
            LOG_ERROR("InstalledScanner: could not clear rename target: %s", dst.c_str());
            return false;
        }
    }

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
