#include "InstallChecker.h"
#include "app/Paths.h"
#include "util/Logger.h"

#include <json.hpp>
#include <sys/stat.h>
#include <fstream>

static bool dirExists(const std::string& path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

InstallStatus InstallChecker::check(const std::string& modId,
                                     const std::string& repoVersion,
                                     const std::vector<std::string>& titleIds) {
    InstallStatus status;
    for (auto& titleId : titleIds) {
        // Check both active (sdcafiine) and disabled paths
        std::vector<std::string> candidates = {
            Paths::sdcafiineBase() + "/" + titleId + "/" + modId,
            Paths::disabledBase()  + "/" + titleId + "/" + modId
        };

        for (auto& path : candidates) {
            if (!dirExists(path)) continue;

            status.installed = true;
            status.titleId   = titleId;

            std::ifstream f(path + "/modinfo.json");
            if (f.is_open()) {
                try {
                    nlohmann::json j;
                    f >> j;
                    if (j.contains("version"))
                        status.installedVersion = j["version"].get<std::string>();
                } catch (...) {
                    LOG_WARN("InstallChecker: failed to parse modinfo.json for %s", modId.c_str());
                }
            }

            status.updateAvail = !repoVersion.empty()
                              && !status.installedVersion.empty()
                              && status.installedVersion != repoVersion;
            return status;
        }
    }
    return status;
}
