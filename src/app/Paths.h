#pragma once
#include <string>

// Central path management.
// On real Wii U: sd:/ is mounted via WHBMountSdCard().
// In Cemu: use /vol/external01/ as SD card equivalent.
namespace Paths {
    extern bool sdMounted;
#ifdef CUPSTORE_TESTS
    extern std::string testRootOverride;
#endif

    inline std::string sdRoot() {
#ifdef CUPSTORE_TESTS
        if (!testRootOverride.empty()) return testRootOverride;
#endif
        return sdMounted ? "fs:/vol/external01" : "/vol/external01";
    }

    inline std::string modstoreBase() {
        return sdRoot() + "/wiiu/apps/coffeeshop";
    }

    inline std::string cacheDir() {
        return modstoreBase() + "/cache";
    }

    // Mods are extracted here first, then atomically renamed into the active
    // sdcafiine path. Failed extracts never leave half-installed mods in the
    // active path.
    inline std::string stagingDir() {
        return cacheDir() + "/staging";
    }

    inline std::string configFile() {
#if BUILD_HW
        return modstoreBase() + "/config.json";  // SD card
#else
        return "/vol/content/config.json";        // wuhb-embedded (Cemu only)
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
