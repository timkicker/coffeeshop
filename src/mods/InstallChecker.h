#pragma once
#include <string>
#include <vector>

struct InstallStatus {
    bool        installed        = false;
    bool        updateAvail      = false;
    std::string installedVersion;
    std::string titleId;          // which titleId folder it's installed under
};

class InstallChecker {
public:
    // Check if a mod is installed under any of the given titleIds.
    // Reads modinfo.json to get installed version, compares with repoVersion.
    static InstallStatus check(const std::string& modId,
                               const std::string& repoVersion,
                               const std::vector<std::string>& titleIds);
};
