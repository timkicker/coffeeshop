#include "InstallHelper.h"
#include "app/Paths.h"

#include <sys/stat.h>
#include <map>

static bool dirExists(const std::string& path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

// Last 2 chars of title ID indicate region
// 00 = JPN, 01 = USA, 02 = EUR, others exist too
std::string InstallHelper::regionName(const std::string& titleId) {
    if (titleId.size() < 2) return titleId;
    std::string suffix = titleId.substr(titleId.size() - 2);
    if (suffix == "00") return "JPN";
    if (suffix == "01") return "USA";
    if (suffix == "02") return "EUR";
    if (suffix == "03") return "AUS";
    if (suffix == "04") return "KOR";
    if (suffix == "05") return "CHN";
    if (suffix == "06") return "TWN";
    return suffix;
}

std::vector<TitleEntry> InstallHelper::detectInstalled(
    const std::vector<std::string>& titleIds)
{
    std::string base = Paths::sdcafiineBase();
    std::vector<TitleEntry> found;

    for (auto& id : titleIds) {
        std::string path = base + "/" + id;
        if (dirExists(path)) {
            found.push_back({id, regionName(id)});
        }
    }

    // None found: return all so user can still pick
    if (found.empty()) {
        for (auto& id : titleIds)
            found.push_back({id, regionName(id)});
    }

    return found;
}
