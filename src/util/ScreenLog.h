#pragma once
#include <string>
#include <vector>
#include <mutex>

// Set to 0 to disable on-screen debug log (e.g. for release builds)
#ifndef SCREENLOG_ENABLED
#define SCREENLOG_ENABLED 1
#endif

#if SCREENLOG_ENABLED

class ScreenLog {
public:
    static ScreenLog& get() { static ScreenLog s; return s; }

    void add(const std::string& msg) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_lines.push_back(msg);
        if (m_lines.size() > 14) m_lines.erase(m_lines.begin());
    }

    std::vector<std::string> lines() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_lines;
    }

private:
    std::vector<std::string> m_lines;
    std::mutex               m_mutex;
};

#define SLOG(x) ScreenLog::get().add(x)

#else

// No-op stubs for release builds
#define SLOG(x) do {} while(0)

// Dummy class so code that calls ScreenLog::get().lines() still compiles
class ScreenLog {
public:
    static ScreenLog& get() { static ScreenLog s; return s; }
    std::vector<std::string> lines() { return {}; }
};

#endif
