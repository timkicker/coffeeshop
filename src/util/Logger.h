#pragma once
#include <cstdio>
#include <cstdarg>
#include <string>
#include <vector>
#include <unistd.h>

class Logger {
public:
    static Logger& get() { static Logger l; return l; }

    void init(const std::string& path) {
        if (m_file) fclose(m_file);
        m_file = fopen(path.c_str(), "w");
        if (m_file) {
            fprintf(m_file, "LOGGER READY\n");
            fflush(m_file);
            fsync(fileno(m_file));
        }
    }

    void log(const char* level, const char* fmt, va_list args) {
        if (!m_file) return;
        fprintf(m_file, "[%s] ", level);
        vfprintf(m_file, fmt, args);
        fprintf(m_file, "\n");
        fflush(m_file);
        fsync(fileno(m_file));
    }

    const std::vector<std::string>& lines() const {
        static std::vector<std::string> empty;
        return empty;
    }

    const std::string& path() const {
        static std::string p;
        return p;
    }

    ~Logger() { if (m_file) fclose(m_file); }

private:
    Logger() = default;
    FILE* m_file = nullptr;
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
