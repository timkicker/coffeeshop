#include "CrashDump.h"

#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>

// Async-signal-safe crash dumper. Keep this file dead simple --
// no allocations, no stdio, no C++ stdlib in the handler.

namespace {

static const char* g_dumpPath = nullptr;

// Write a NUL-terminated C string to fd, signal-safe.
static void writeStr(int fd, const char* s) {
    if (!s) return;
    size_t n = 0;
    while (s[n]) n++;
    ssize_t r = write(fd, s, n);
    (void)r; // silence unused-result
}

// Write a small integer (positive only) to fd, signal-safe.
static void writeInt(int fd, int v) {
    if (v < 0) { writeStr(fd, "-"); v = -v; }
    char buf[16];
    int  pos = 0;
    if (v == 0) { buf[pos++] = '0'; }
    else {
        while (v > 0) { buf[pos++] = '0' + (v % 10); v /= 10; }
    }
    // reverse into final buffer
    char out[16];
    for (int i = 0; i < pos; i++) out[i] = buf[pos - 1 - i];
    ssize_t r = write(fd, out, pos);
    (void)r;
}

static const char* signalName(int signo) {
    switch (signo) {
        case SIGSEGV: return "SIGSEGV";
        case SIGBUS:  return "SIGBUS";
        case SIGFPE:  return "SIGFPE";
        case SIGILL:  return "SIGILL";
        case SIGABRT: return "SIGABRT";
        default:       return "?";
    }
}

extern "C" void crashHandler(int signo) {
    if (!g_dumpPath) {
        // Fallback: re-raise default behavior to terminate.
        signal(signo, SIG_DFL);
        raise(signo);
        return;
    }
    int fd = open(g_dumpPath, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd >= 0) {
        writeStr(fd, "CRASH signal=");
        writeStr(fd, signalName(signo));
        writeStr(fd, " (");
        writeInt(fd, signo);
        writeStr(fd, ")\n");
        writeStr(fd, "If you can reproduce, please file an issue with this file.\n");
        // PowerPC has no portable backtrace API in WUT; the signal+context
        // alone tells us a lot. For deeper info we'd need WUT-specific dumps.
        close(fd);
    }
    // Re-raise default to actually terminate.
    signal(signo, SIG_DFL);
    raise(signo);
}

} // namespace

namespace CrashDump {

void install(const char* path) {
    g_dumpPath = path;
    // WUT/newlib doesn't ship sigaction(); the older signal() API is enough
    // here -- the handler restores SIG_DFL before raise(), so single-shot
    // semantics are preserved manually.
    signal(SIGSEGV, crashHandler);
    signal(SIGBUS,  crashHandler);
    signal(SIGFPE,  crashHandler);
    signal(SIGILL,  crashHandler);
    signal(SIGABRT, crashHandler);
}

const char* lastDumpPath() {
    return g_dumpPath;
}

} // namespace CrashDump
