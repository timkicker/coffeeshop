#include <whb/proc.h>
#include <whb/log.h>
#include <whb/log_udp.h>

#include "app/App.h"
#include "app/Paths.h"
#include "util/Logger.h"

// SD card mounting - try WHBMountSdCard if available at runtime
#ifdef __WUT__
#include <sys/iosupport.h>
extern "C" {
    // Forward declare - provided by wut at link time
    bool WHBMountSdCard();
    bool WHBUnmountSdCard();
}
#endif

int main(int argc, char** argv) {
    WHBProcInit();
    WHBLogUdpInit();

#ifdef __WUT__
    Paths::sdMounted = false;
    if (Paths::sdMounted) {
        LOG_INFO("SD card mounted at sd:/");
    } else {
        LOG_WARN("SD card not available, using /vol/content fallback");
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

    WHBLogUdpDeinit();
    WHBProcShutdown();
    return 0;
}
