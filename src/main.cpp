#include <whb/proc.h>
#include <whb/log.h>
#include "app/App.h"
#include "app/Paths.h"
#include "util/Logger.h"

#if !BUILD_HW
#include <whb/log_udp.h>
#endif

#ifdef __WUT__
#include <sys/iosupport.h>
extern "C" {
    bool WHBMountSdCard();
    bool WHBUnmountSdCard();
}
#endif

int main(int argc, char** argv) {
    WHBProcInit();

#if !BUILD_HW
    // UDP logging only for Cemu/debug builds
    WHBLogUdpInit();
#endif

#ifdef __WUT__
    Paths::sdMounted = false; // WHBMountSdCard only on hw
    if (Paths::sdMounted) {
        LOG_INFO("SD card mounted");
    } else {
        LOG_WARN("SD card mount failed, using /vol/content fallback");
    }
#else
    Paths::sdMounted = false;
#endif

    App app;
    if (app.init()) {
        app.run();
    }

#ifdef __WUT__
    if (Paths::sdMounted) WHBUnmountSdCard();
#endif

#if !BUILD_HW
    WHBLogUdpDeinit();
#endif

    WHBProcShutdown();
    return 0;
}
