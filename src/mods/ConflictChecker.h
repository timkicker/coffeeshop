#pragma once
#include "mods/InstalledScanner.h"
#include <string>
#include <vector>

struct ConflictResult {
    bool                     hasConflict = false;
    std::vector<std::string> conflictingMods; // names of mods that conflict
    std::vector<std::string> conflictingFiles; // first few conflicting file paths
};

class ConflictChecker {
public:
    // Check if activating `mod` would conflict with any currently active mod
    // Returns conflict info - caller decides whether to proceed
    static ConflictResult check(const InstalledMod& mod,
                                const std::vector<InstalledMod>& allMods);

private:
    // Recursively collect all relative file paths under a mod folder
    static std::vector<std::string> collectFiles(const std::string& modPath);
    static void collectFilesRec(const std::string& base,
                                const std::string& rel,
                                std::vector<std::string>& out);
};
