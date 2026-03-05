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
    m_running = true;
    m_worker  = std::thread(&DownloadQueue::workerLoop, this);
}

void DownloadQueue::stop() {
    m_running = false;
    m_cv.notify_all();
    if (m_worker.joinable()) m_worker.join();
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

        processJob(m_jobs[pendingIdx]);
    }
}

void DownloadQueue::processJob(DownloadJob& job) {
    std::string tmpPath = Paths::cacheDir() + "/" + job.mod.id + ".zip";
    std::string destDir = Paths::sdcafiineBase() + "/" + job.titleId + "/" + job.mod.id;

    mkdirp(Paths::cacheDir());

    DownloadManager dm;
    dm.run(job.mod.download, tmpPath, destDir);

    std::lock_guard<std::mutex> lock(m_mutex);
    if (dm.state() == DownloadManager::State::Done) {
        writeModInfo(destDir, job.mod);
        job.state        = DownloadJob::State::Done;
        job.progress     = 1.0f;
        job.hasFinishedAt = true;
        job.finishedAt   = std::chrono::steady_clock::now();
        LOG_INFO("DownloadQueue: done: %s", job.mod.name.c_str());
    } else {
        job.state        = DownloadJob::State::Error;
        job.error        = dm.error();
        job.hasFinishedAt = true;
        job.finishedAt   = std::chrono::steady_clock::now();
        LOG_ERROR("DownloadQueue: error: %s", job.error.c_str());
    }
}
