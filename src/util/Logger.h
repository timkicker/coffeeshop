#pragma once
#include <cstdio>
#include <cstdarg>
#include <mutex>
#include <string>
#include <vector>

class Logger {
public:
    static Logger& get() { static Logger l; return l; }

    void init(const std::string& path) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_file) fclose(m_file);
        m_file = fopen(path.c_str(), "w");
        m_path = path;
    }

    void log(const char* level, const char* fmt, va_list args) {
        std::lock_guard<std::mutex> lock(m_mutex);

        char buf[512];
        vsnprintf(buf, sizeof(buf), fmt, args);

        // stdout
        fprintf(stdout, "[%s] %s\n", level, buf);

        // file
        if (m_file) {
            fprintf(m_file, "[%s] %s\n", level, buf);
            fflush(m_file);
        }

        // in-memory ring buffer for log viewer (last 200 lines)
        std::string line = std::string("[") + level + "] " + buf;
        m_lines.push_back(line);
        if (m_lines.size() > 200) m_lines.erase(m_lines.begin());
    }

    const std::vector<std::string>& lines() const { return m_lines; }
    const std::string& path() const { return m_path; }

    ~Logger() { if (m_file) fclose(m_file); }

private:
    Logger() = default;
    FILE*                    m_file = nullptr;
    std::string              m_path;
    std::vector<std::string> m_lines;
    std::mutex               m_mutex;
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
