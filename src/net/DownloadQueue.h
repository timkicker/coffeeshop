#pragma once

#include "net/RepoManager.h"
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <chrono>

struct DownloadJob {
    enum class State { Pending, Downloading, Extracting, Done, Error };

    Mod         mod;
    std::string titleId;
    State       state    = State::Pending;
    float       progress = 0.0f;
    std::string error;
    std::chrono::steady_clock::time_point finishedAt;
    bool        hasFinishedAt = false;
};

class DownloadQueue {
public:
    static DownloadQueue& get() { static DownloadQueue q; return q; }

    void enqueue(const Mod& mod, const std::string& titleId);
    std::vector<DownloadJob> jobs();
    int activeCount();
    void dismissError(int index); // Remove error job at index
    void start();
    void stop();

private:
    DownloadQueue() = default;
    void workerLoop();
    void processJob(DownloadJob& job);
    void cleanupFinished();

    std::vector<DownloadJob> m_jobs;
    std::mutex               m_mutex;
    std::condition_variable  m_cv;
    std::thread              m_worker;
    std::atomic<bool>        m_running { false };
    std::atomic<bool>        m_cancelDownload { false };

    static constexpr int CLEANUP_SECONDS = 30;
};
