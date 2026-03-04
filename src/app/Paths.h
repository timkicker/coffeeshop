#pragma once
#include <string>

// Central path management.
// On real Wii U: sd:/ is mounted via WHBMountSdCard().
// In Cemu: use /vol/external01/ as SD card equivalent.
namespace Paths {
    extern bool sdMounted;

    inline std::string sdRoot() {
        return sdMounted ? "sd:" : "/vol/external01";
    }

    inline std::string modstoreBase() {
        return sdRoot() + "/wiiu/apps/coffeeshop";
    }

    inline std::string cacheDir() {
        return modstoreBase() + "/cache";
    }

    inline std::string configFile() {
#if BUILD_HW
        return modstoreBase() + "/config.json";
#else
        return "/vol/content/config.json";
#endif
    }

    inline std::string sdcafiineBase() {
        return sdRoot() + "/wiiu/sdcafiine";
    }

    // Disabled mods live here - outside sdcafiine so the plugin ignores them
    inline std::string disabledBase() {
        return modstoreBase() + "/disabled";
    }
}
