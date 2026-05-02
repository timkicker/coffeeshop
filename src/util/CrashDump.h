#pragma once

// Install signal handlers for SIGSEGV / SIGBUS / SIGFPE / SIGILL that write
// a minimal crash report to SD before the process dies. Must be called as
// early as possible in main() (after fopen for the dump path is viable).
//
// The handler is async-signal-safe: only write() and integer formatting --
// no malloc, no fprintf, no fopen. If the SD path can't be opened at install
// time, signals are still installed but the dump is silent (no harm done).
namespace CrashDump {
    // path: absolute path to dump file (e.g. /vol/external01/.../crash.log)
    void install(const char* path);

    // Returns the most recently written crash dump if there was one this
    // session boundary; empty string otherwise. Reads from the dump path.
    // Call ONCE at startup AFTER install() to detect last-session crash.
    const char* lastDumpPath();
}
