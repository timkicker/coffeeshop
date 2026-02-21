#pragma once
#include <string>

namespace Paths {
    extern bool sdMounted;

    inline std::string modstoreBase() {
        return sdMounted ? "sd:/wiiu/apps/modstore" : "/vol/save/modstore";
    }
    inline std::string cacheDir() {
        return "/vol/external01/wiiu/apps/modstore/cache";
    }
    inline std::string configFile() {
#if BUILD_HW
        return modstoreBase() + "/config.json";
#else
        return "/vol/content/config.json";  // Cemu: bundled in .wuhb
#endif
    }
    inline std::string sdcafiineBase() {
        return sdMounted ? "sd:/wiiu/sdcafiine" : "/vol/external01/wiiu/sdcafiine";
    }
}
