extern void elog(const char* msg);
extern void elogf(const char* fmt, ...);
#include "App.h"
#include "app/Input.h"
#include "app/Config.h"
#include "app/Paths.h"
#include "ui/Screen.h"
#include "ui/MainLayout.h"
#include "util/Logger.h"
#include <cstdarg>
#include <cerrno>
#include <sys/stat.h>
#include "net/DownloadQueue.h"
#include <sysapp/launch.h>
#include "util/ImageCache.h"
#include "util/TextCache.h"
#include "app/CacheManager.h"

#include <SDL2/SDL.h>
#include <whb/proc.h>
#include <proc_ui/procui.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>

#ifdef __WUT__
#include <nn/ac.h>
extern "C" {
    void socket_lib_init();
}
#endif

App::App() = default;

App::~App() {
    elog("~App: DownloadQueue stop");
    DownloadQueue::get().stop();
    elog("~App: clearing screens");
    m_screens.clear();
    elog("~App: ImageCache shutdown");
    if (m_renderer) ImageCache::get().shutdown(m_renderer);
    elog("~App: TextCache clear");
    TextCache::get().clear();
    elog("~App: DestroyRenderer");
    if (m_renderer) SDL_DestroyRenderer(m_renderer);
    elog("~App: DestroyWindow");
    if (m_window)   SDL_DestroyWindow(m_window);
    elog("~App: IMG_Quit");
    IMG_Quit();
    elog("~App: TTF_Quit");
    TTF_Quit();
    elog("~App: SDL_Quit");
    SDL_Quit();
    elog("~App: done");
}

bool App::init() {
    elog("Network init...");
#ifdef __WUT__
    // Initialize the WUT network stack on both hardware AND Cemu builds.
    // Without these calls, Cemu's emulated socket layer never finishes
    // setup and curl connect() hangs. The previous BUILD_HW gate broke
    // cemu mode entirely (app stuck on "Loading repos...").
    nn::ac::Initialize();
    nn::ac::Connect();
    socket_lib_init();
    elog("Network ready");
#endif

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER) != 0) {
        LOG_ERROR("SDL_Init failed: %s", SDL_GetError());
        return false;
    }
    elog("SDL_Init OK");
    if (TTF_Init() != 0) {
        LOG_ERROR("TTF_Init failed: %s", TTF_GetError());
        return false;
    }
    elog("TTF_Init OK");
    if (!IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG)) {
        LOG_ERROR("IMG_Init failed: %s", IMG_GetError());
        return false;
    }
    elog("IMG_Init OK");

    elogf("requesting window %dx%d", m_screenW, m_screenH);
    m_window = SDL_CreateWindow(
        "Wii U Mod Store",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        m_screenW, m_screenH,
        SDL_WINDOW_SHOWN
    );
    if (!m_window) {
        LOG_ERROR("SDL_CreateWindow failed: %s", SDL_GetError());
        elogf("SDL_CreateWindow failed: %s", SDL_GetError());
        return false;
    }
    int wW, wH;
    SDL_GetWindowSize(m_window, &wW, &wH);
    elogf("Window OK: actual size %dx%d (requested %dx%d)", wW, wH, m_screenW, m_screenH);

    m_renderer = SDL_CreateRenderer(m_window, -1, SDL_RENDERER_ACCELERATED);
    if (!m_renderer) {
        elogf("ACCELERATED renderer failed: %s -- trying SOFTWARE", SDL_GetError());
        m_renderer = SDL_CreateRenderer(m_window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (!m_renderer) {
        LOG_ERROR("SDL_CreateRenderer failed: %s", SDL_GetError());
        elogf("SDL_CreateRenderer failed entirely: %s", SDL_GetError());
        return false;
    }
    SDL_RendererInfo info;
    if (SDL_GetRendererInfo(m_renderer, &info) == 0) {
        elogf("Renderer OK: '%s' flags=0x%x maxTex=%dx%d",
              info.name ? info.name : "?",
              info.flags, info.max_texture_width, info.max_texture_height);
    } else {
        elog("Renderer OK (info query failed)");
    }
    int rW, rH;
    SDL_GetRendererOutputSize(m_renderer, &rW, &rH);
    elogf("Renderer output size: %dx%d", rW, rH);

    elog("before logger");
    if (mkdir((Paths::modstoreBase()).c_str(), 0755) != 0 && errno != EEXIST) {
        elog("mkdir for app dir failed - logger may not work");
    }
    // Pre-create the disabled-mods folder so InstalledScanner doesn't log
    // "opendir failed" on every scan() when nothing has been deactivated yet.
    mkdir(Paths::disabledBase().c_str(), 0755);
    elog("mkdir done");
    Logger::get().init(Paths::modstoreBase() + "/app.log");
    elog("logger init done");
    LOG_INFO("App started");
    elog("before pushScreen");
    pushScreen(std::make_unique<MainLayout>(this));
    elog("after pushScreen");
    elog("cleanupStaleZips...");
    CacheManager::cleanupStaleZips();
    elog("cleanupStalePartials...");
    CacheManager::cleanupStalePartials();
    elog("cleanupStaleStaging...");
    CacheManager::cleanupStaleStaging();
    // cleanupCorruptMods deliberately not run at startup:
    //  1. It blocks boot for minutes on real hardware -- rmrf on a large mod
    //     folder (e.g. hundreds of custom-track files) takes too long on SD.
    //  2. It silently deletes user data (any folder lacking modinfo.json)
    //     which is hostile to mods installed manually outside the app.
    // InstalledScanner already handles missing modinfo.json gracefully.
    // If we ever want this back, it should be a manual Settings action with
    // confirmation, not a startup auto-delete.
    elog("DownloadQueue::start (2 workers)...");
    DownloadQueue::get().setWorkerCount(2);
    DownloadQueue::get().start();
    elog("DownloadQueue started");
    elog("ImageCache::start...");
    ImageCache::get().start();
    elog("ImageCache started");
    elog("App::init returning true");

    return true;
}

void App::run() {
    m_running = true;
    int pruneCounter = 0;
    int frameNum = 0;
    elogf("App::run entering -- screens=%zu running=%d procRunning=%d",
          m_screens.size(), (int)m_running,
#ifdef __WUT__
          (int)WHBProcIsRunning()
#else
          1
#endif
    );

    while (m_running && !m_screens.empty() && WHBProcIsRunning()) {
        if (frameNum < 10 || frameNum % 60 == 0) {
            elogf("frame %d begin", frameNum);
        }
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                elog("got SDL_QUIT");
                m_running = false;
            }
        }
        if (frameNum < 5) elog("  before update");
        update();
        if (frameNum < 5) elog("  after update");
        render();
        if (frameNum < 5) elogf("  after render (sdl_err='%s')", SDL_GetError());
        TextCache::get().tick();
        if (++pruneCounter > 300) {
            TextCache::get().prune();
            pruneCounter = 0;
            elogf("frame %d: pruned text cache", frameNum);
        }
        frameNum++;
    }
    elogf("App::run loop exited -- frames=%d running=%d screens=%zu procRunning=%d",
          frameNum, (int)m_running, m_screens.size(),
#ifdef __WUT__
          (int)WHBProcIsRunning()
#else
          1
#endif
    );
#ifdef __WUT__
    // Drain ProcUI until Aroma confirms exit - required for WHBProcShutdown to work
    while (WHBProcIsRunning()) {}
#endif
}

void App::quit() {
    m_running = false;
}
void App::startExit() {
    m_exiting = true;
    m_running = false;
#ifdef __WUT__
    SYSLaunchMenu();
#endif
}

void App::pushScreen(std::unique_ptr<Screen> screen) {
    screen->onEnter();
    m_screens.push_back(std::move(screen));
}

void App::popScreen() {
    if (!m_screens.empty()) {
        m_screens.back()->onExit();
        m_screens.pop_back();
    }
    if (m_screens.empty()) m_running = false;
}

void App::update() {
    if (!m_screens.empty()) {
        Input input = Input::read();
        m_screens.back()->handleInput(input);
        m_screens.back()->update();
    }
}

void App::render() {
    static int s_renderCount = 0;
    bool diag = (s_renderCount < 3);
    if (diag) elogf("App::render #%d -- renderer=%p screens=%zu",
                    s_renderCount, (void*)m_renderer, m_screens.size());
    if (!m_renderer) { if (diag) elog("  renderer is NULL!"); return; }

    // Clear stale SDL error so per-frame elog doesn't carry forward an
    // unrelated message (e.g. "Not a PNG" from a one-time IMG_Load_RW failure
    // that propagates across hundreds of frames).
    SDL_ClearError();

    int rc = SDL_SetRenderDrawColor(m_renderer, 15, 15, 25, 255);
    if (diag) elogf("  SetDrawColor=%d err='%s'", rc, SDL_GetError());
    rc = SDL_RenderClear(m_renderer);
    if (diag) elogf("  RenderClear=%d err='%s'", rc, SDL_GetError());

    if (!m_screens.empty()) {
        if (diag) elog("  calling screen->render");
        m_screens.back()->render(m_renderer);
        if (diag) elogf("  screen->render returned err='%s'", SDL_GetError());
    } else {
        if (diag) elog("  m_screens empty, skipping screen render");
    }
    SDL_RenderPresent(m_renderer);
    if (diag) elogf("  RenderPresent returned err='%s'", SDL_GetError());
    s_renderCount++;
}
