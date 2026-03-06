#pragma once
#include <string>
#include <atomic>
#include <curl/curl.h>

class DownloadManager {
public:
    enum class State { Idle, Downloading, Extracting, Done, Error };

    void setCancelFlag(std::atomic<bool>* f) { m_cancelFlag = f; }
    void run(const std::string& zipUrl,
             const std::string& tmpPath,
             const std::string& destDir,
             int maxRetries = 2);

    State       state()    const { return m_state.load(); }
    float       progress() const { return m_progress.load(); }
    std::string error()    const { return m_error; }

private:
    bool download(const std::string& zipUrl, const std::string& tmpPath);
    bool validateZip(const std::string& path);
    bool checkDiskSpace(const std::string& dir, uint64_t needed);

    static int curlProgress(void* userdata, curl_off_t total, curl_off_t now,
                            curl_off_t, curl_off_t);

    std::atomic<State> m_state    { State::Idle };
    std::atomic<float> m_progress { 0.0f };
    std::string        m_error;
    std::atomic<bool>* m_cancelFlag { nullptr };
};
