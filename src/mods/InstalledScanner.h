#pragma once

#include <string>
#include <vector>

struct InstalledMod {
    std::string id;        // folder name (e.g. "hd-ui-pack")
    std::string titleId;   // parent folder (e.g. "000500001010EB00")
    std::string name;      // from modinfo.json, falls back to id
    std::string version;   // from modinfo.json
    std::string repoVersion; // from repo (empty if not matched)
    bool        active;    // true if no .disabled_ prefix
    std::string path;      // full path to mod folder
};

class InstalledScanner {
public:
    // Scan sdcafiineBase() for installed mods
    static std::vector<InstalledMod> scan();

    // Toggle active/inactive by renaming folder
    static bool setActive(InstalledMod& mod, bool active);

    // Delete mod folder entirely
    static bool remove(const InstalledMod& mod);

    // Check if update available (repoVersion != version && !repoVersion.empty())
    static bool hasUpdate(const InstalledMod& mod);
};
