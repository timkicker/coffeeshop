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

    DownloadJob() = default;
    DownloadJob(const DownloadJob& o)
        : mod(o.mod), titleId(o.titleId), state(o.state),
          progress(o.progress), error(o.error),
          finishedAt(o.finishedAt), hasFinishedAt(o.hasFinishedAt),
          cancel(o.cancel.load()) {}
    DownloadJob(DownloadJob&& o) noexcept
        : mod(std::move(o.mod)), titleId(std::move(o.titleId)), state(o.state),
          progress(o.progress), error(std::move(o.error)),
          finishedAt(o.finishedAt), hasFinishedAt(o.hasFinishedAt),
          cancel(o.cancel.load()) {}
    DownloadJob& operator=(DownloadJob&& o) noexcept {
        mod = std::move(o.mod); titleId = std::move(o.titleId);
        state = o.state; progress = o.progress;
        error = std::move(o.error); finishedAt = o.finishedAt;
        hasFinishedAt = o.hasFinishedAt; cancel.store(o.cancel.load());
        return *this;
    }
    DownloadJob& operator=(const DownloadJob&) = delete;

    Mod         mod;
    std::string titleId;
    State       state    = State::Pending;
    float       progress = 0.0f;
    std::string error;
    std::chrono::steady_clock::time_point finishedAt;
    bool        hasFinishedAt = false;
    std::atomic<bool> cancel { false };
};

class DownloadQueue {
public:
    static DownloadQueue& get() { static DownloadQueue q; return q; }

    void enqueue(const Mod& mod, const std::string& titleId);
    std::vector<DownloadJob> jobs();
    int activeCount();
    void cancelJob(int index);
    void dismissError(int index); // Remove error job at index
    void start();
    void stop();
#ifdef CUPSTORE_TESTS
    void _testReset(); // Clears all jobs - test-only
#endif

private:
    DownloadQueue() = default;
    void workerLoop();
    void processJob(int idx, const Mod& mod,
                    const std::string& titleId,
                    std::atomic<bool>* cancelFlag);
    void cleanupFinished();

    std::vector<DownloadJob> m_jobs;
    std::mutex               m_mutex;
    std::condition_variable  m_cv;
    std::thread              m_worker;
    std::atomic<bool>        m_running { false };

    static constexpr int CLEANUP_SECONDS = 30;
};
