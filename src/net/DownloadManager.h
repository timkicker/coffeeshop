#pragma once
#include <string>
#include <atomic>
#include <curl/curl.h>

class DownloadManager {
public:
    enum class State { Idle, Downloading, Verifying, Extracting, Done, Error };

    void setCancelFlag(std::atomic<bool>* f) { m_cancelFlag = f; }
    void setExpectedHash(const std::string& sha256Hex) { m_expectedHash = sha256Hex; }
    void run(const std::string& zipUrl,
             const std::string& tmpPath,
             const std::string& destDir,
             int maxRetries = 2);

    State       state()    const { return m_state.load(); }
    float       progress() const { return m_progress.load(); }
    std::string error()    const { return m_error; }

    // Public utilities (also used by tests)
    static bool validateZip(const std::string& path);
    static bool checkDiskSpace(const std::string& dir, uint64_t needed);
    static bool rmrf(const std::string& path);

private:
    bool download(const std::string& zipUrl, const std::string& tmpPath);

    static int curlProgress(void* userdata, curl_off_t total, curl_off_t now,
                            curl_off_t, curl_off_t);

    std::atomic<State> m_state    { State::Idle };
    std::atomic<float> m_progress { 0.0f };
    std::string        m_error;
    std::atomic<bool>* m_cancelFlag { nullptr };
    std::string        m_expectedHash; // empty = skip verify
};
