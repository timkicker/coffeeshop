#include "CacheManager.h"
#include "app/Paths.h"
#include "util/Logger.h"
#include "net/DownloadManager.h"

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstring>
#include <ctime>
#include <vector>
#include <string>

void CacheManager::cleanupStaleZips() {
    std::string dir = Paths::cacheDir();
    DIR* d = opendir(dir.c_str());
    if (!d) return; // Cache dir doesn't exist yet, nothing to clean

    int removed = 0;
    struct dirent* e;
    while ((e = readdir(d)) != nullptr) {
        std::string name = e->d_name;
        if (name.size() < 4) continue;
        if (name.substr(name.size() - 4) != ".zip") continue;
        std::string path = dir + "/" + name;
        if (remove(path.c_str()) == 0) {
            LOG_INFO("CacheManager: removed stale zip: %s", name.c_str());
            removed++;
        }
    }
    closedir(d);
    if (removed > 0)
        LOG_INFO("CacheManager: cleaned up %d stale zip(s)", removed);
}

void CacheManager::cleanupStalePartials() {
    std::string dir = Paths::cacheDir();
    DIR* d = opendir(dir.c_str());
    if (!d) return;

    int removed = 0;
    time_t now = time(nullptr);
    constexpr time_t kMaxAgeSec = 24 * 60 * 60; // 24h

    struct dirent* e;
    while ((e = readdir(d)) != nullptr) {
        std::string name = e->d_name;
        if (name.size() < 8) continue;
        if (name.substr(name.size() - 8) != ".partial") continue;
        std::string path = dir + "/" + name;
        struct stat st;
        if (stat(path.c_str(), &st) != 0) continue;
        if (now - st.st_mtime > kMaxAgeSec) {
            if (remove(path.c_str()) == 0) {
                LOG_INFO("CacheManager: removed stale .partial: %s", name.c_str());
                removed++;
            }
        }
    }
    closedir(d);
    if (removed > 0)
        LOG_INFO("CacheManager: cleaned up %d stale partial(s)", removed);
}

void CacheManager::cleanupStaleStaging() {
    // 1) Delete every subdirectory of cache/staging.
    std::string staging = Paths::stagingDir();
    DIR* d = opendir(staging.c_str());
    if (d) {
        int removed = 0;
        struct dirent* e;
        while ((e = readdir(d)) != nullptr) {
            std::string name = e->d_name;
            if (name == "." || name == "..") continue;
            if (DownloadManager::rmrf(staging + "/" + name)) removed++;
        }
        closedir(d);
        if (removed > 0)
            LOG_INFO("CacheManager: cleared %d stale staging entr%s",
                     removed, removed == 1 ? "y" : "ies");
    }

    // 2) Delete any "<modid>.old" folders in active sdcafiine paths
    //    (leftover from a crashed atomic-rename swap).
    std::vector<std::string> bases = { Paths::sdcafiineBase(), Paths::disabledBase() };
    for (auto& base : bases) {
        DIR* td = opendir(base.c_str());
        if (!td) continue;
        struct dirent* te;
        while ((te = readdir(td)) != nullptr) {
            std::string titleId = te->d_name;
            if (titleId == "." || titleId == "..") continue;
            std::string titlePath = base + "/" + titleId;
            DIR* md = opendir(titlePath.c_str());
            if (!md) continue;
            struct dirent* me;
            while ((me = readdir(md)) != nullptr) {
                std::string entry = me->d_name;
                if (entry == "." || entry == "..") continue;
                if (entry.size() > 4 && entry.substr(entry.size() - 4) == ".old") {
                    std::string oldPath = titlePath + "/" + entry;
                    if (DownloadManager::rmrf(oldPath))
                        LOG_INFO("CacheManager: removed stale .old: %s", oldPath.c_str());
                }
            }
            closedir(md);
        }
        closedir(td);
    }
}

void CacheManager::cleanupCorruptMods() {
    // Check both active and disabled base dirs
    std::vector<std::string> bases = {
        Paths::sdcafiineBase(),
        Paths::disabledBase()
    };

    int removed = 0;

    for (auto& base : bases) {
        DIR* titleDir = opendir(base.c_str());
        if (!titleDir) continue;

        struct dirent* titleEntry;
        while ((titleEntry = readdir(titleDir)) != nullptr) {
            std::string titleId = titleEntry->d_name;
            if (titleId == "." || titleId == "..") continue;

            std::string titlePath = base + "/" + titleId;
            DIR* modDir = opendir(titlePath.c_str());
            if (!modDir) continue;

            struct dirent* modEntry;
            while ((modEntry = readdir(modDir)) != nullptr) {
                std::string modId = modEntry->d_name;
                if (modId == "." || modId == "..") continue;

                std::string modPath   = titlePath + "/" + modId;
                std::string modinfoPath = modPath + "/modinfo.json";

                // Check if modinfo.json exists
                struct stat st;
                if (stat(modinfoPath.c_str(), &st) == 0) continue; // ok

                // No modinfo.json - corrupt/partial install, remove it.
                // Use the proper recursive rmrf -- the inline single-level
                // delete here used to leave nested subdirectories behind.
                LOG_WARN("CacheManager: corrupt mod folder (no modinfo.json): %s/%s - removing",
                         titleId.c_str(), modId.c_str());
                if (DownloadManager::rmrf(modPath)) removed++;
            }
            closedir(modDir);
        }
        closedir(titleDir);
    }

    if (removed > 0)
        LOG_INFO("CacheManager: removed %d corrupt mod folder(s)", removed);
}
