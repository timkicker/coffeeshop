#include "CacheManager.h"
#include "app/Paths.h"
#include "util/Logger.h"

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstring>
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

                // No modinfo.json - corrupt/partial install, remove it
                LOG_WARN("CacheManager: corrupt mod folder (no modinfo.json): %s/%s - removing",
                         titleId.c_str(), modId.c_str());

                // rmrf the mod folder
                // Simple recursive delete
                std::string cmd = modPath; // we'll do it manually
                DIR* d2 = opendir(modPath.c_str());
                if (d2) {
                    struct dirent* f;
                    while ((f = readdir(d2)) != nullptr) {
                        std::string fn = f->d_name;
                        if (fn == "." || fn == "..") continue;
                        std::string fp = modPath + "/" + fn;
                        remove(fp.c_str());
                    }
                    closedir(d2);
                }
                if (rmdir(modPath.c_str()) == 0) removed++;
            }
            closedir(modDir);
        }
        closedir(titleDir);
    }

    if (removed > 0)
        LOG_INFO("CacheManager: removed %d corrupt mod folder(s)", removed);
}
