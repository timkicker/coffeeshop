#pragma once
#include "mods/InstalledScanner.h"
#include <string>
#include <vector>

struct ConflictResult {
    bool                     hasConflict = false;
    std::vector<std::string> conflictingMods;  // names of mods that conflict
    std::vector<std::string> conflictingFiles; // first few conflicting file paths
};

class ConflictChecker {
public:
    // High-level: scans mod folders on disk (requires filesystem)
    static ConflictResult check(const InstalledMod& mod,
                                const std::vector<InstalledMod>& allMods);

    // Pure / testable: takes pre-collected file lists
    // modFiles: relative paths of the mod being checked
    // others: list of {name, files} for active mods to compare against
    struct ModFiles {
        std::string              name;
        std::vector<std::string> files;
    };
    static ConflictResult checkFiles(const std::vector<std::string>& modFiles,
                                     const std::vector<ModFiles>& others);

private:
    static std::vector<std::string> collectFiles(const std::string& modPath);
    static void collectFilesRec(const std::string& base,
                                const std::string& rel,
                                std::vector<std::string>& out);
};
