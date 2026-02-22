#include "ConflictChecker.h"
#include "util/Logger.h"

#include <dirent.h>
#include <sys/stat.h>
#include <algorithm>
#include <unordered_set>

static bool isDir(const std::string& path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

void ConflictChecker::collectFilesRec(const std::string& base,
                                       const std::string& rel,
                                       std::vector<std::string>& out) {
    std::string full = rel.empty() ? base : base + "/" + rel;
    DIR* d = opendir(full.c_str());
    if (!d) return;
    struct dirent* e;
    while ((e = readdir(d)) != nullptr) {
        std::string name = e->d_name;
        if (name == "." || name == "..") continue;
        if (name == "modinfo.json") continue;
        std::string relChild  = rel.empty() ? name : rel + "/" + name;
        std::string fullChild = base + "/" + relChild;
        if (isDir(fullChild)) collectFilesRec(base, relChild, out);
        else                  out.push_back(relChild);
    }
    closedir(d);
}

std::vector<std::string> ConflictChecker::collectFiles(const std::string& modPath) {
    std::vector<std::string> files;
    collectFilesRec(modPath, "", files);
    return files;
}

// Pure logic - no filesystem access
ConflictResult ConflictChecker::checkFiles(const std::vector<std::string>& modFiles,
                                            const std::vector<ModFiles>& others) {
    ConflictResult result;
    if (modFiles.empty()) return result;

    std::unordered_set<std::string> mySet(modFiles.begin(), modFiles.end());

    for (auto& other : others) {
        for (auto& f : other.files) {
            if (mySet.count(f)) {
                result.hasConflict = true;
                if (std::find(result.conflictingMods.begin(),
                              result.conflictingMods.end(), other.name) == result.conflictingMods.end())
                    result.conflictingMods.push_back(other.name);
                if (result.conflictingFiles.size() < 3)
                    result.conflictingFiles.push_back(f);
            }
        }
    }
    return result;
}

// Filesystem wrapper - calls checkFiles internally
ConflictResult ConflictChecker::check(const InstalledMod& mod,
                                       const std::vector<InstalledMod>& allMods) {
    auto myFiles = collectFiles(mod.path);

    std::vector<ModFiles> others;
    for (auto& other : allMods) {
        if (other.id == mod.id) continue;
        if (!other.active) continue;
        if (other.titleId != mod.titleId) continue;
        others.push_back({other.name.empty() ? other.id : other.name,
                          collectFiles(other.path)});
    }

    return checkFiles(myFiles, others);
}
