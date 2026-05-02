#include <whb/proc.h>
#include <sysapp/launch.h>
#include <whb/log.h>
#include "app/App.h"
#include "app/Paths.h"
#include "util/Logger.h"
#include "util/CrashDump.h"
#include "audio/AudioManager.h"
#include <whb/log_udp.h>
#include <cstdio>
#include <unistd.h>
#include <sys/stat.h>

#ifdef __WUT__
#include <sys/iosupport.h>
#include <nn/ac.h>
#include <nsysnet/socket.h>
extern "C" {
    bool WHBMountSdCard();
    bool WHBUnmountSdCard();
    void socket_lib_finish();
}
#endif

static const char* g_elog_path = nullptr;
void elog(const char* msg) {
    WHBLogPrintf("[elog] %s", msg);
    if (g_elog_path) {
        FILE* f = fopen(g_elog_path, "a");
        if (f) {
            fprintf(f, "%s\n", msg);
            fclose(f);
        }
    }
}

// printf-style elog. Use for any value-bearing diagnostic line.
#include <cstdarg>
void elogf(const char* fmt, ...) {
    char buf[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    elog(buf);
}

int main(int argc, char** argv) {
    // Diagnostic: prove main() was called before ANY WHB/SDL init.
    {
        FILE* probe = fopen("/vol/external01/wiiu/apps/coffeeshop/main_called.txt", "w");
        if (probe) {
            fprintf(probe, "main reached\n");
            fclose(probe);
        }
    }

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

    // On hardware the SD mounts at "fs:/vol/external01/..." (Aroma WHB mount).
    // In Cemu the prefix is invalid -- pick whichever opens.
    {
        FILE* probe = fopen("fs:/vol/external01/wiiu/apps/coffeeshop/early.log", "w");
        if (probe) {
            fclose(probe);
            g_elog_path = "fs:/vol/external01/wiiu/apps/coffeeshop/early.log";
        } else {
            probe = fopen("/vol/external01/wiiu/apps/coffeeshop/early.log", "w");
            if (probe) {
                fclose(probe);
                g_elog_path = "/vol/external01/wiiu/apps/coffeeshop/early.log";
            }
        }
    }
    elog("START");
    elog(Paths::sdMounted ? "SD mounted" : "SD failed");

    // Crash dump catcher: writes a minimal report on SIGSEGV/etc so users
    // can attach it to a bug report. Must be installed early but after the
    // SD mount above so the dump path is writable.
    static const char* CRASH_PATH = "fs:/vol/external01/wiiu/apps/coffeeshop/crash.log";
    {
        // Detect a previous crash before installing fresh handlers.
        struct stat st;
        if (stat(CRASH_PATH, &st) == 0) {
            elog("PREVIOUS RUN CRASHED -- crash.log present. Read it for details.");
            // Rename to crash.log.prev so we can detect future crashes cleanly.
            std::string prev = std::string(CRASH_PATH) + ".prev";
            rename(CRASH_PATH, prev.c_str());
        }
    }
    CrashDump::install(CRASH_PATH);
    elog("crash handler armed");

    elog("before App init");
    {
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
        elog("app scope end - SDL cleanup follows");
    } // App destructor runs here: SDL, renderer, window all freed
    elog("END");

#ifdef __WUT__
    elog("unmounting SD");
    if (Paths::sdMounted) WHBUnmountSdCard();
#endif
    WHBLogUdpDeinit();
#ifdef __WUT__
    elog("socket_lib_finish");
    socket_lib_finish();
    elog("ac::Finalize");
    nn::ac::Finalize();
#endif
    // elog uses open+write+close per call; nothing to fclose at exit.
    // WHBProcShutdown intentionally omitted (CLAUDE.md): App::run drains
    // ProcUI and SYSLaunchMenu hands control back to the OS.
    return 0;
}
