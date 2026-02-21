#pragma once

#include "net/RepoManager.h"
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>

struct DownloadJob {
    enum class State { Pending, Downloading, Extracting, Done, Error };

    Mod         mod;
    std::string titleId;
    State       state    = State::Pending;
    float       progress = 0.0f;
    std::string error;
};

class DownloadQueue {
public:
    static DownloadQueue& get() { static DownloadQueue q; return q; }

    // Add a job to the queue. Thread-safe.
    void enqueue(const Mod& mod, const std::string& titleId);

    // Snapshot of all jobs (for rendering). Thread-safe.
    std::vector<DownloadJob> jobs();

    // Number of pending + active jobs
    int activeCount();

    // Start the worker thread (call once at app init)
    void start();

    // Stop the worker thread (call at app shutdown)
    void stop();

private:
    DownloadQueue() = default;

    void workerLoop();
    void processJob(DownloadJob& job);

    std::vector<DownloadJob>    m_jobs;
    std::mutex                  m_mutex;
    std::condition_variable     m_cv;
    std::thread                 m_worker;
    std::atomic<bool>           m_running { false };
};
