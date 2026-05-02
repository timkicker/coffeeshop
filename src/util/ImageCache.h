#pragma once

#include <string>
#include <unordered_map>
#include <queue>
#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <vector>

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

// Async image loader. Thread-safe.
// Usage:
//   ImageCache::get().request(url);          // start download
//   SDL_Texture* t = ImageCache::get().texture(url, renderer); // get when ready
class ImageCache {
public:
    static ImageCache& get() { static ImageCache c; return c; }

    // Start the background worker thread.
    void start();

    // Stop the worker thread. Safe to call multiple times.
    void stop();

    // Request an image to be downloaded. No-op if already cached/loading.
    void request(const std::string& url);

    // Returns texture if ready, nullptr if still loading or failed.
    // Creates SDL_Texture on first call after data is ready (must be called from main thread).
    SDL_Texture* texture(const std::string& url, SDL_Renderer* renderer);

    // Free all textures and clear cached entries. Keeps the worker alive so
    // subsequent request() calls still get serviced (used from Settings ->
    // "Clear cache"). Pass renderer for symmetry; not currently used.
    void clear(SDL_Renderer* renderer);

    // Full shutdown: clear() + stop worker + free SDL textures. Call ONLY
    // from App::~App, before SDL_DestroyRenderer.
    void shutdown(SDL_Renderer* renderer);

private:
    ImageCache() = default;

    enum class State { Idle, Loading, Ready, Failed };

    struct Entry {
        State                state = State::Idle;
        std::vector<uint8_t> data;   // raw downloaded bytes
        SDL_Texture*         tex  = nullptr;
    };

    void workerLoop();
    void download(const std::string& url);

    std::unordered_map<std::string, Entry> m_cache;
    std::mutex                             m_mutex;

    std::queue<std::string>  m_queue;
    std::condition_variable  m_cv;
    std::thread              m_worker;
    std::atomic<bool>        m_running { false };
};
