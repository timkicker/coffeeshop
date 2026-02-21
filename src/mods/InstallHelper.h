#pragma once
#include <string>
#include <vector>

// Known title ID to region name mappings
struct TitleEntry {
    std::string id;
    std::string region; // "JPN", "USA", "EUR", etc.
};

class InstallHelper {
public:
    // Filter title IDs to those that have an existing sdcafiine folder.
    // If none exist, returns all (so user can still install).
    static std::vector<TitleEntry> detectInstalled(
        const std::vector<std::string>& titleIds);

    // Map a title ID to a human-readable region name
    static std::string regionName(const std::string& titleId);
};
