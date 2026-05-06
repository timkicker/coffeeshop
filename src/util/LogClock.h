#pragma once
// Uptime-relative logging clock. Both early.log (elog) and app.log (Logger)
// timestamp lines with seconds-since-process-start, so a single timeline
// can be reconstructed from either file. Wall-clock would be nicer but a
// Wii U that's been off for a long time has a wildly wrong RTC.

#ifdef __WUT__
#include <coreinit/time.h>
namespace LogClock {
    inline OSTime& epoch() { static OSTime e = 0; return e; }
    inline void init() { epoch() = OSGetSystemTime(); }
    inline double elapsedSec() {
        return (double)OSTicksToMicroseconds(OSGetSystemTime() - epoch()) / 1e6;
    }
}
#else
#include <chrono>
namespace LogClock {
    inline std::chrono::steady_clock::time_point& epoch() {
        static auto e = std::chrono::steady_clock::now();
        return e;
    }
    inline void init() { epoch() = std::chrono::steady_clock::now(); }
    inline double elapsedSec() {
        return std::chrono::duration<double>(
            std::chrono::steady_clock::now() - epoch()).count();
    }
}
#endif
