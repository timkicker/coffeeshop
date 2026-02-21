#pragma once
#include <string>
#include <curl/curl.h>
#include <atomic>
#include <functional>

class DownloadManager {
public:
    enum class State { Idle, Downloading, Extracting, Done, Error };

    // Download zipUrl, save to tmpPath, then extract to destDir.
    // Call from a background thread.
    void run(const std::string& zipUrl,
             const std::string& tmpPath,
             const std::string& destDir);

    State       state()    const { return m_state.load(); }
    float       progress() const { return m_progress.load(); }  // 0.0 - 1.0
    std::string error()    const { return m_error; }

private:
    static int curlProgress(void* userdata, curl_off_t total, curl_off_t now,
                            curl_off_t, curl_off_t);

    std::atomic<State> m_state    { State::Idle };
    std::atomic<float> m_progress { 0.0f };
    std::string        m_error;
};
