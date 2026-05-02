#pragma once
#include <cstdio>
#include <cstdarg>
#include <deque>
#include <mutex>
#include <string>
#include <vector>
#include <unistd.h>

class Logger {
public:
    static Logger& get() { static Logger l; return l; }

    void init(const std::string& path) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_file) fclose(m_file);
        m_file = fopen(path.c_str(), "w");
        m_path = path;
        if (m_file) {
            fprintf(m_file, "LOGGER READY\n");
            fflush(m_file);
            fsync(fileno(m_file));
            pushLine_locked("LOGGER READY");
        }
    }

    void log(const char* level, const char* fmt, va_list args) {
        std::lock_guard<std::mutex> lock(m_mutex);
        // File output (always)
        if (m_file) {
            fprintf(m_file, "[%s] ", level);
            // vfprintf consumes args, so capture once for both file + buffer
            va_list args_copy;
            va_copy(args_copy, args);
            vfprintf(m_file, fmt, args);
            fprintf(m_file, "\n");
            fflush(m_file);
            fsync(fileno(m_file));

            // In-memory rolling buffer for the in-app log viewer
            char buf[1024];
            int n = snprintf(buf, sizeof(buf), "[%s] ", level);
            if (n > 0 && n < (int)sizeof(buf))
                vsnprintf(buf + n, sizeof(buf) - n, fmt, args_copy);
            pushLine_locked(buf);
            va_end(args_copy);
        }
    }

    // Snapshot copy of recent lines (rolling window). Safe to call from any thread.
    std::vector<std::string> lines() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return std::vector<std::string>(m_buffer.begin(), m_buffer.end());
    }

    const std::string& path() const { return m_path; }

    ~Logger() {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_file) fclose(m_file);
    }

private:
    Logger() = default;

    void pushLine_locked(const std::string& s) {
        m_buffer.push_back(s);
        if ((int)m_buffer.size() > kMaxLines) m_buffer.pop_front();
    }

    static constexpr int kMaxLines = 500;

    FILE*                 m_file = nullptr;
    std::string           m_path;
    std::deque<std::string> m_buffer;
    mutable std::mutex    m_mutex;
};

inline void _log(const char* level, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    Logger::get().log(level, fmt, args);
    va_end(args);
}

#define LOG_INFO(fmt, ...)  _log("INFO ",  fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)  _log("WARN ",  fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) _log("ERROR",  fmt, ##__VA_ARGS__)
