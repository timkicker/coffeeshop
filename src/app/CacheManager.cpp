#include "CacheManager.h"
#include "app/Paths.h"
#include "util/Logger.h"

#include <dirent.h>
#include <cstring>
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
