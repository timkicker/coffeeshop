#pragma once
#include <string>
#include <vector>
#include <mutex>

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
