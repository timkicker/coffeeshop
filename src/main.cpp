#include <whb/proc.h>
#include <whb/log.h>
#include "app/App.h"
#include "app/Paths.h"
#include "util/Logger.h"
#include "audio/AudioManager.h"
#include <whb/log_udp.h>
#include <cstdio>
#include <unistd.h>

#ifdef __WUT__
#include <sys/iosupport.h>
extern "C" {
    bool WHBMountSdCard();
    bool WHBUnmountSdCard();
}
#endif

static FILE* g_elog = nullptr;
void elog(const char* msg) {
    if (g_elog) { 
        fprintf(g_elog, "%s\n", msg); 
        fflush(g_elog);
        fsync(fileno(g_elog));
    }
}

int main(int argc, char** argv) {
    WHBProcInit();
    WHBLogUdpInit();

#ifdef __WUT__
#if BUILD_HW
    Paths::sdMounted = WHBMountSdCard();
#else
    Paths::sdMounted = false;
#endif
#else
    Paths::sdMounted = false;
#endif

    g_elog = fopen("fs:/vol/external01/wiiu/apps/coffeeshop/early.log", "w");
    elog("START");
    elog(Paths::sdMounted ? "SD mounted" : "SD failed");

    elog("before App init");
    App app;
    elog("App created");

    if (app.init()) {
        elog("App init OK");
        AudioManager::get().init();
        elog("Audio init OK");
        app.run();
        elog("run returned");
        AudioManager::get().shutdown();
    } else {
        elog("App init FAILED");
    }

    elog("END");
    if (g_elog) fclose(g_elog);

#ifdef __WUT__
    if (Paths::sdMounted) WHBUnmountSdCard();
#endif
    WHBLogUdpDeinit();
    WHBProcShutdown();
    return 0;
}
