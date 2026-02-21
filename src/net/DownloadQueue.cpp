#include "DownloadQueue.h"
#include "net/DownloadManager.h"
#include "app/Paths.h"
#include "util/Logger.h"

#include <sys/stat.h>

static void mkdirp(const std::string& path) {
    for (size_t i = 1; i <= path.size(); i++) {
        if (i == path.size() || path[i] == '/') {
            mkdir(path.substr(0, i).c_str(), 0755);
        }
    }
}

void DownloadQueue::enqueue(const Mod& mod, const std::string& titleId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    DownloadJob job;
    job.mod     = mod;
    job.titleId = titleId;
    job.state   = DownloadJob::State::Pending;
    m_jobs.push_back(std::move(job));
    m_cv.notify_one();
    LOG_INFO("DownloadQueue: enqueued %s for %s", mod.name.c_str(), titleId.c_str());
}

std::vector<DownloadJob> DownloadQueue::jobs() {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_jobs;
}

int DownloadQueue::activeCount() {
    std::lock_guard<std::mutex> lock(m_mutex);
    int count = 0;
    for (auto& j : m_jobs)
        if (j.state == DownloadJob::State::Pending ||
            j.state == DownloadJob::State::Downloading ||
            j.state == DownloadJob::State::Extracting)
            count++;
    return count;
}

void DownloadQueue::start() {
    m_running = true;
    m_worker  = std::thread(&DownloadQueue::workerLoop, this);
    LOG_INFO("DownloadQueue: worker started");
}

void DownloadQueue::stop() {
    m_running = false;
    m_cv.notify_all();
    if (m_worker.joinable()) m_worker.join();
    LOG_INFO("DownloadQueue: worker stopped");
}

void DownloadQueue::workerLoop() {
    while (m_running) {
        DownloadJob* pending = nullptr;
        int pendingIdx = -1;

        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_cv.wait(lock, [this] {
                if (!m_running) return true;
                for (auto& j : m_jobs)
                    if (j.state == DownloadJob::State::Pending) return true;
                return false;
            });

            if (!m_running) break;

            for (int i = 0; i < (int)m_jobs.size(); i++) {
                if (m_jobs[i].state == DownloadJob::State::Pending) {
                    pendingIdx = i;
                    break;
                }
            }
        }

        if (pendingIdx < 0) continue;

        // Process without holding lock (updates progress inline)
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_jobs[pendingIdx].state = DownloadJob::State::Downloading;
        }

        processJob(m_jobs[pendingIdx]);
    }
}

void DownloadQueue::processJob(DownloadJob& job) {
    std::string tmpPath = Paths::cacheDir() + "/" + job.mod.id + ".zip";
    std::string destDir = Paths::sdcafiineBase() + "/" + job.titleId + "/" + job.mod.id;

    // Ensure dirs
    mkdirp(Paths::cacheDir());
    mkdirp(destDir);

    // Use DownloadManager inline but update job state directly
    DownloadManager dm;

    // Run in this thread (we are already the worker thread)
    dm.run(job.mod.download, tmpPath, destDir);

    // Map DownloadManager state back to job state
    // We poll progress while running - but dm.run() is blocking.
    // For progress updates we need a slightly different approach:
    // DownloadManager updates m_progress atomically so we mirror it.

    std::lock_guard<std::mutex> lock(m_mutex);
    if (dm.state() == DownloadManager::State::Done) {
        job.state    = DownloadJob::State::Done;
        job.progress = 1.0f;
        LOG_INFO("DownloadQueue: job done: %s", job.mod.name.c_str());
    } else {
        job.state = DownloadJob::State::Error;
        job.error = dm.error();
        LOG_ERROR("DownloadQueue: job error: %s", job.error.c_str());
    }
}
