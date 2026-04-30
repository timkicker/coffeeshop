#include "DownloadQueue.h"
#include "net/DownloadManager.h"
#include "app/Paths.h"
#include "util/Logger.h"

#include <sys/stat.h>
#include <json.hpp>
#include <fstream>

static void mkdirp(const std::string& path) {
    for (size_t i = 1; i <= path.size(); i++) {
        if (i == path.size() || path[i] == '/')
            mkdir(path.substr(0, i).c_str(), 0755);
    }
}

static void writeModInfo(const std::string& destDir, const Mod& mod) {
    nlohmann::json j;
    j["id"]      = mod.id;
    j["version"] = mod.version;
    // Simple date via __DATE__ not ideal but no <ctime> issues on WUT
    j["installedAt"] = __DATE__;

    std::string path = destDir + "/modinfo.json";
    std::ofstream f(path);
    if (f.is_open()) f << j.dump(2);
}

void DownloadQueue::enqueue(const Mod& mod, const std::string& titleId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    DownloadJob job;
    job.mod     = mod;
    job.titleId = titleId;
    job.state   = DownloadJob::State::Pending;
    m_jobs.push_back(std::move(job));
    m_cv.notify_one();
    LOG_INFO("DownloadQueue: enqueued %s", mod.name.c_str());
}

std::vector<DownloadJob> DownloadQueue::jobs() {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_jobs;
}

int DownloadQueue::activeCount() {
    std::lock_guard<std::mutex> lock(m_mutex);
    int count = 0;
    for (auto& j : m_jobs)
        if (j.state == DownloadJob::State::Pending   ||
            j.state == DownloadJob::State::Downloading ||
            j.state == DownloadJob::State::Extracting)
            count++;
    return count;
}

void DownloadQueue::start() {
    if (m_running.exchange(true)) return; // already started
    int n = (m_workerCount >= 1) ? m_workerCount : 1;
    m_workers.reserve(n);
    for (int i = 0; i < n; i++)
        m_workers.emplace_back(&DownloadQueue::workerLoop, this);
}

void DownloadQueue::stop() {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto& j : m_jobs) j.cancel = true;
    }
    m_running = false;
    m_cv.notify_all();
    for (auto& w : m_workers) if (w.joinable()) w.join();
    m_workers.clear();
}

void DownloadQueue::cancelJob(int index) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (index >= 0 && index < (int)m_jobs.size()) {
        auto& j = m_jobs[index];
        if (j.state == DownloadJob::State::Pending) {
            j.state        = DownloadJob::State::Error;
            j.error        = "Cancelled";
            j.hasFinishedAt = true;
            j.finishedAt   = std::chrono::steady_clock::now();
        } else if (j.state == DownloadJob::State::Downloading ||
                   j.state == DownloadJob::State::Extracting) {
            j.cancel = true;
        }
    }
}

void DownloadQueue::cleanupFinished() {
    auto now = std::chrono::steady_clock::now();
    m_jobs.erase(
        std::remove_if(m_jobs.begin(), m_jobs.end(), [&](const DownloadJob& j) {
            // Only auto-remove Done jobs - Error jobs stay until dismissed by user
            if (j.state != DownloadJob::State::Done) return false;
            if (!j.hasFinishedAt) return false;
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                now - j.finishedAt).count();
            return elapsed >= CLEANUP_SECONDS;
        }),
        m_jobs.end()
    );
}

void DownloadQueue::dismissError(int index) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (index >= 0 && index < (int)m_jobs.size() &&
        m_jobs[index].state == DownloadJob::State::Error)
        m_jobs.erase(m_jobs.begin() + index);
}

#ifdef CUPSTORE_TESTS
void DownloadQueue::_testReset() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_jobs.clear();
}
#endif

void DownloadQueue::workerLoop() {
    while (m_running) {
        int pendingIdx = -1;

        {
            std::unique_lock<std::mutex> lock(m_mutex);
            lock.unlock();
            // Poll instead of condition_variable (more reliable on WUT)
            for (int i = 0; i < 50 && m_running; i++) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                std::lock_guard<std::mutex> pl(m_mutex);
                for (auto& j : m_jobs)
                    if (j.state == DownloadJob::State::Pending) goto done_waiting;
            }
            done_waiting:
            lock.lock();

            if (!m_running) break;

            cleanupFinished();

            for (int i = 0; i < (int)m_jobs.size(); i++) {
                if (m_jobs[i].state == DownloadJob::State::Pending) {
                    pendingIdx = i;
                    m_jobs[i].state = DownloadJob::State::Downloading;
                    break;
                }
            }
        }

        if (pendingIdx < 0) continue;

        // Copy input data while locked; cancel flag stays in the vector
        // entry (safe: only Error jobs are erased, and ours is Downloading).
        Mod jobMod;
        std::string jobTitleId;
        std::atomic<bool>* jobCancel = nullptr;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            jobMod     = m_jobs[pendingIdx].mod;
            jobTitleId = m_jobs[pendingIdx].titleId;
            jobCancel  = &m_jobs[pendingIdx].cancel;
        }

        processJob(pendingIdx, jobMod, jobTitleId, jobCancel);
    }
}

void DownloadQueue::processJob(int idx, const Mod& mod,
                                const std::string& titleId,
                                std::atomic<bool>* cancelFlag) {
    std::string tmpPath = Paths::cacheDir() + "/" + mod.id + ".zip";
    std::string destDir = Paths::sdcafiineBase() + "/" + titleId + "/" + mod.id;

    mkdirp(Paths::cacheDir());

    DownloadManager dm;
    dm.setCancelFlag(cancelFlag);
    dm.setExpectedHash(mod.sha256);
    dm.run(mod.download, tmpPath, destDir);

    std::lock_guard<std::mutex> lock(m_mutex);
    if (idx < 0 || idx >= (int)m_jobs.size()) return;

    auto& job = m_jobs[idx];
    if (dm.state() == DownloadManager::State::Done) {
        writeModInfo(destDir, mod);
        job.state        = DownloadJob::State::Done;
        job.progress     = 1.0f;
        job.hasFinishedAt = true;
        job.finishedAt   = std::chrono::steady_clock::now();
        LOG_INFO("DownloadQueue: done: %s", mod.name.c_str());
    } else {
        job.state        = DownloadJob::State::Error;
        job.error        = cancelFlag->load() ? "Cancelled" : dm.error();
        job.hasFinishedAt = true;
        job.finishedAt   = std::chrono::steady_clock::now();
        LOG_ERROR("DownloadQueue: error: %s", job.error.c_str());
    }
}
