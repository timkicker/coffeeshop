#pragma once
#include <string>
#include <vector>

struct InstalledMod {
    std::string id;
    std::string titleId;
    std::string name;        // from modinfo.json, falls back to id
    std::string version;     // from modinfo.json
    std::string repoVersion; // from repo (empty if not matched)
    bool        active;      // true = in sdcafiine/, false = in disabled/
    std::string path;        // full path to mod folder
};

class InstalledScanner {
public:
    // Scan both sdcafiineBase() and disabledBase() for installed mods
    static std::vector<InstalledMod> scan();

    // Move mod between sdcafiine/ and disabled/
    static bool setActive(InstalledMod& mod, bool active);

    // Delete mod folder entirely (from wherever it currently lives)
    static bool remove(const InstalledMod& mod);

    static bool hasUpdate(const InstalledMod& mod);
};
