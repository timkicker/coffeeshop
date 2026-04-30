extern void elog(const char* msg);
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
    elog("~App: ImageCache clear");
    if (m_renderer) ImageCache::get().clear(m_renderer);
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

    m_window = SDL_CreateWindow(
        "Wii U Mod Store",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        m_screenW, m_screenH,
        SDL_WINDOW_SHOWN
    );
    if (!m_window) {
        LOG_ERROR("SDL_CreateWindow failed: %s", SDL_GetError());
        return false;
    }
    elog("Window OK");

    m_renderer = SDL_CreateRenderer(m_window, -1, SDL_RENDERER_ACCELERATED);
    if (!m_renderer) {
        LOG_ERROR("SDL_CreateRenderer failed: %s", SDL_GetError());
        return false;
    }
    elog("Renderer OK");

    elog("before logger");
    if (mkdir((Paths::modstoreBase()).c_str(), 0755) != 0 && errno != EEXIST) {
        elog("mkdir for app dir failed - logger may not work");
    }
    elog("mkdir done");
    Logger::get().init(Paths::modstoreBase() + "/app.log");
    elog("logger init done");
    LOG_INFO("App started");
    elog("before pushScreen");
    pushScreen(std::make_unique<MainLayout>(this));
    elog("after pushScreen");
    CacheManager::cleanupStaleZips();
    CacheManager::cleanupCorruptMods();
    DownloadQueue::get().start();
    ImageCache::get().start();

    return true;
}

void App::run() {
    m_running = true;
    int pruneCounter = 0;
    while (m_running && !m_screens.empty() && WHBProcIsRunning()) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) m_running = false;
        }
        update();
        render();
        TextCache::get().tick();
        // Prune unused text textures every ~5 seconds (at 60fps).
        if (++pruneCounter > 300) { TextCache::get().prune(); pruneCounter = 0; }
    }
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
    SDL_SetRenderDrawColor(m_renderer, 15, 15, 25, 255);
    SDL_RenderClear(m_renderer);
    if (!m_screens.empty()) {
        m_screens.back()->render(m_renderer);
    }
    SDL_RenderPresent(m_renderer);
}
