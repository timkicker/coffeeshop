#include "MainLayout.h"
#include "app/App.h"
#include "ui/DetailScreen.h"
#include "ui/DownloadQueueScreen.h"
#include "ui/TagFilterScreen.h"
#include "ui/BrowseScroll.h"
#include "net/DownloadQueue.h"
#include <sysapp/launch.h>
#include "mods/InstalledScanner.h"
#include "util/Logger.h"
#include "util/TextCache.h"
#include "app/Paths.h"
#include <sys/statvfs.h>
#include <sys/stat.h>
#include <dirent.h>
#include <algorithm>
#include "util/ImageCache.h"
#include "audio/AudioManager.h"
extern void elog(const char* msg);
extern void elogf(const char* fmt, ...);

static constexpr const char* FONT_PATH = "/vol/content/fonts/Roboto-Regular.ttf";
static constexpr int CARDS_PER_ROW = 3;
static constexpr int CARD_W        = 280;
static constexpr int CARD_H        = 160;
static constexpr int CARD_PAD      = 20;
static constexpr int GRID_TOP      = 70;

MainLayout::MainLayout(App* app) : Screen(app) {}

MainLayout::~MainLayout() {
    elog("~MainLayout: start");
    m_stopFetch = true;
    elog("~MainLayout: joining fetchThread");
    if (m_fetchThread.joinable()) m_fetchThread.join();
    elog("~MainLayout: join done");
    elog("~MainLayout: closing fonts");
    if (m_fontNormal) TTF_CloseFont(m_fontNormal);
    if (m_fontSmall)  TTF_CloseFont(m_fontSmall);
    if (m_fontTiny)   TTF_CloseFont(m_fontTiny);
    elog("~MainLayout: done");
}

void MainLayout::onEnter() {
    elogf("onEnter start -- font path: %s", FONT_PATH);
    m_fontNormal = TTF_OpenFont(FONT_PATH, 26);
    m_fontSmall  = TTF_OpenFont(FONT_PATH, 18);
    m_fontTiny   = TTF_OpenFont(FONT_PATH, 14);
    elogf("fonts done: normal=%p small=%p tiny=%p (TTF err='%s')",
          (void*)m_fontNormal, (void*)m_fontSmall, (void*)m_fontTiny, TTF_GetError());
    m_config.load();
    m_showOnboarding = !m_config.hasRepos();
    // Restore persisted UI state
    if      (m_config.sortMode == "name")    m_sortMode = SortMode::NameAZ;
    else if (m_config.sortMode == "version") m_sortMode = SortMode::Version;
    else                                      m_sortMode = SortMode::Default;
    m_activeTags.clear();
    for (auto& t : m_config.activeTags) m_activeTags.insert(t);
    if      (m_config.lastTab == "installed") m_activeTab = Tab::Installed;
    else if (m_config.lastTab == "settings")  m_activeTab = Tab::Settings;
    else                                       m_activeTab = Tab::Browse;
    if (!m_showOnboarding && !m_config.repos.empty()) {
        m_fetchState = FetchState::Loading;
        elog("config done");
    m_fetchThread = std::thread([this]() {
        LOG_INFO("Fetch thread started");
            Repo combined;
            std::string lastError;

            int totalRepos = (int)m_config.repos.size();
            int repoIdx    = 0;
            for (auto& url : m_config.repos) {
                if (m_stopFetch) break;
                repoIdx++;
                {
                    std::lock_guard<std::mutex> sl(m_repoMutex);
                    // Show short host portion in progress label
                    size_t hostStart = url.find("://");
                    hostStart = (hostStart == std::string::npos) ? 0 : hostStart + 3;
                    size_t hostEnd = url.find('/', hostStart);
                    std::string host = url.substr(hostStart,
                                                   hostEnd == std::string::npos ? url.size() - hostStart
                                                                                : hostEnd - hostStart);
                    m_fetchProgress = std::to_string(repoIdx) + "/" +
                                      std::to_string(totalRepos) + "  " + host;
                }
                LOG_INFO("Processing repo: %s", url.c_str());
                RepoManager rm;
                rm.fetch(url, &m_stopFetch);
                {
                    std::lock_guard<std::mutex> sl(m_repoMutex);
                    m_repoStatus[url] = rm.lastError().empty() ? "OK" : rm.lastError();
                }
                if (!rm.lastError().empty()) {
                    lastError = url + ": " + rm.lastError();
                    LOG_WARN("MainLayout: repo failed: %s", lastError.c_str());
                    continue;
                }
                // Merge games from this repo into combined
                for (auto& g : rm.repo().games){
                    LOG_INFO("Merging %zu games from repo", rm.repo().games.size());
                    combined.games.push_back(g);
                }
            }

            if (m_stopFetch) return;
            std::lock_guard<std::mutex> lock(m_repoMutex);
            LOG_INFO("Fetch loop done, %zu total games", combined.games.size());
            m_repo = combined;
            if (combined.games.empty()) {
                m_fetchError = lastError.empty() ? "No repos returned any mods" : lastError;
                m_fetchState = FetchState::Error;
                LOG_INFO("Starting conflict check...");
            } else {
                m_fetchState = FetchState::Done;
                LOG_INFO("Conflict check done, %zu conflicts", m_startupConflicts.size());
            }
        });
            LOG_INFO("Fetch thread ending");
    }
}

void MainLayout::onExit() {}

void MainLayout::persistUiState() {
    m_config.sortMode = (m_sortMode == SortMode::NameAZ) ? "name"
                      : (m_sortMode == SortMode::Version) ? "version"
                                                          : "default";
    m_config.lastTab = (m_activeTab == Tab::Installed) ? "installed"
                     : (m_activeTab == Tab::Settings)  ? "settings"
                                                        : "browse";
    m_config.activeTags.assign(m_activeTags.begin(), m_activeTags.end());
    m_config.save();
}

void MainLayout::refreshInstalled() {
    m_installedMods = InstalledScanner::scan();

    // Match repo versions
    std::lock_guard<std::mutex> lock(m_repoMutex);
    for (auto& inst : m_installedMods) {
        for (auto& game : m_repo.games) {
            for (auto& mod : game.mods) {
                if (mod.id == inst.id) {
                    inst.repoVersion = mod.version;
                    inst.name        = mod.name; // use display name from repo
                    break;
                }
            }
        }
    }
    m_installedDirty = false;
    if (m_selectedInstalled >= (int)m_installedMods.size())
        m_selectedInstalled = std::max(0, (int)m_installedMods.size() - 1);
}

void MainLayout::handleInput(const Input& input) {
    if (input.minus) {
        // Stop and join the fetch thread before exiting -- detach() risks the
        // background thread accessing m_repo / m_repoStatus after the screen
        // is destroyed.
        LOG_INFO("MainLayout: minus pressed, signalling fetch thread to stop");
        m_stopFetch = true;
        if (m_fetchThread.joinable()) {
            LOG_INFO("MainLayout: joining fetch thread...");
            m_fetchThread.join();
            LOG_INFO("MainLayout: fetch thread joined");
        }
        LOG_INFO("MainLayout: calling App::startExit");
        m_app->startExit();
        return;
    }
    if (m_showOnboarding) {
        if (input.a) { m_config.load(); m_showOnboarding = !m_config.hasRepos(); }
        return;
    }

    if (input.plus) {
        m_app->pushScreen(std::make_unique<DownloadQueueScreen>(m_app));
        return;
    }

    if (input.l) {
        Tab before = m_activeTab;
        if      (m_activeTab == Tab::Installed) { m_activeTab = Tab::Browse;    AudioManager::get().playSound(SoundId::Navigate); }
        else if (m_activeTab == Tab::Settings)  { m_activeTab = Tab::Installed; AudioManager::get().playSound(SoundId::Navigate); }
        if (before != m_activeTab) {
            LOG_INFO("MainLayout: tab L -> %d", (int)m_activeTab);
            persistUiState();
        }
    }
    if (input.r) {
        Tab before = m_activeTab;
        if      (m_activeTab == Tab::Browse)    { m_activeTab = Tab::Installed; AudioManager::get().playSound(SoundId::Navigate); }
        else if (m_activeTab == Tab::Installed) { m_activeTab = Tab::Settings;  m_settingsDirty = true; AudioManager::get().playSound(SoundId::Navigate); }
        if (before != m_activeTab) {
            LOG_INFO("MainLayout: tab R -> %d", (int)m_activeTab);
            persistUiState();
        }
    }

    if (m_activeTab == Tab::Browse)    handleBrowseInput(input);
    if (m_activeTab == Tab::Installed) handleInstalledInput(input);
    if (m_activeTab == Tab::Settings)  handleSettingsInput(input);
}

void MainLayout::handleBrowseInput(const Input& input) {
    std::lock_guard<std::mutex> lock(m_repoMutex);
    auto& games = m_repo.games;
    if (games.empty()) return;

    auto& mods     = games[m_selectedGame].mods;
    int   modCount = (int)mods.size();
    int   cols     = CARDS_PER_ROW;
    int   prevGame = m_selectedGame;

    if (input.right) {
        if ((m_selectedMod % cols) < cols - 1 && m_selectedMod + 1 < modCount) {
            m_selectedMod++; AudioManager::get().playSound(SoundId::Navigate);
        } else if (m_selectedGame < (int)games.size() - 1) {
            m_selectedGame++; m_selectedMod = 0; AudioManager::get().playSound(SoundId::Navigate);
        }
    }
    if (input.left) {
        if ((m_selectedMod % cols) > 0) {
            m_selectedMod--; AudioManager::get().playSound(SoundId::Navigate);
        } else if (m_selectedMod == 0 && m_selectedGame > 0) {
            m_selectedGame--; m_selectedMod = 0; AudioManager::get().playSound(SoundId::Navigate);
        }
    }
    if (input.down) { int n = m_selectedMod + cols; if (n < modCount) { m_selectedMod = n; AudioManager::get().playSound(SoundId::Navigate); } }
    if (input.up)   { int p = m_selectedMod - cols; if (p >= 0)       { m_selectedMod = p; AudioManager::get().playSound(SoundId::Navigate); } }

    // Reset scroll when the selected game changes, then ensure the new cursor
    // is visible. Otherwise just adjust scroll if cursor moved off the visible
    // rows. visibleRows is derived from the actual window height minus the
    // grid top (header/tab strip) and a small bottom strip for the hint line.
    if (m_selectedGame != prevGame) m_browseScrollRow = 0;
    const int H = m_app->screenHeight();
    const int bottomHint = 30;
    int visibleRows = (H - GRID_TOP - bottomHint) / (CARD_H + CARD_PAD);
    if (visibleRows < 1) visibleRows = 1;
    int selRow = m_selectedMod / cols;
    m_browseScrollRow = computeBrowseScroll(m_browseScrollRow, selRow, visibleRows);

    // Pre-warm ImageCache for the currently-selected mod's images. When the
    // user presses A, DetailScreen finds them already cached -> no freeze.
    if (m_selectedMod < modCount) {
        const Mod& sel = games[m_selectedGame].mods[m_selectedMod];
        if (!sel.thumbnail.empty()) ImageCache::get().request(sel.thumbnail);
        for (auto& s : sel.screenshots) ImageCache::get().request(s);
    }

    if (input.a && !mods.empty()) {
        AudioManager::get().playSound(SoundId::Navigate);
        const auto& game = games[m_selectedGame];
        m_app->pushScreen(std::make_unique<DetailScreen>(
            m_app, game.mods[m_selectedMod], game.name, game.titleIds));
    }

    // X cycles sort mode
    if (input.x) {
        m_sortMode = (m_sortMode == SortMode::Default)  ? SortMode::NameAZ
                   : (m_sortMode == SortMode::NameAZ)   ? SortMode::Version
                                                        : SortMode::Default;
        AudioManager::get().playSound(SoundId::Navigate);
        persistUiState();
    }

    // Y opens tag filter overlay
    if (input.y) {
        auto tags = RepoManager::aggregateTags(m_repo);
        m_app->pushScreen(std::make_unique<TagFilterScreen>(
            m_app, std::move(tags), m_activeTags,
            [this](std::set<std::string> picked) {
                m_activeTags = std::move(picked);
                m_selectedMod = 0; // reset cursor when filter changes
                m_browseScrollRow = 0;
                persistUiState();
            }));
    }
}

void MainLayout::handleInstalledInput(const Input& input) {
    if (m_installedMods.empty()) return;

    // Update-all confirmation dialog
    if (m_confirmUpdateAll) {
        if (input.a) {
            // Enqueue downloads for every mod that has an update available.
            // Match by mod.id within the cached repo.
            std::lock_guard<std::mutex> lock(m_repoMutex);
            int queued = 0;
            for (auto& inst : m_installedMods) {
                if (!InstalledScanner::hasUpdate(inst)) continue;
                for (auto& g : m_repo.games) {
                    bool found = false;
                    for (auto& mod : g.mods) {
                        if (mod.id == inst.id) {
                            DownloadQueue::get().enqueue(mod, inst.titleId);
                            queued++;
                            found = true;
                            break;
                        }
                    }
                    if (found) break;
                }
            }
            LOG_INFO("Update-all: enqueued %d updates", queued);
            AudioManager::get().playSound(SoundId::DownloadStart);
            m_confirmUpdateAll = false;
        }
        if (input.b) m_confirmUpdateAll = false;
        return;
    }

    // Conflict warning dialog
    if (m_showConflict) {
        if (input.a) {
            // Proceed anyway
            InstalledScanner::setActive(m_installedMods[m_selectedInstalled], true);
            AudioManager::get().playSound(SoundId::ModActivated);
            m_installedDirty = true;
            m_showConflict   = false;
        }
        if (input.b) m_showConflict = false;
        return;
    }

    // Confirm uninstall dialog
    if (m_confirmUninstall) {
        if (input.a) {
            InstalledScanner::remove(m_installedMods[m_selectedInstalled]);
            m_confirmUninstall = false;
            m_installedDirty   = true;
        }
        if (input.b) m_confirmUninstall = false;
        return;
    }

    if (input.up)   { int prev = m_selectedInstalled; m_selectedInstalled = std::max(0, m_selectedInstalled - 1); if (m_selectedInstalled != prev) AudioManager::get().playSound(SoundId::Navigate); }
    if (input.down) { int prev = m_selectedInstalled; m_selectedInstalled = std::min((int)m_installedMods.size()-1, m_selectedInstalled + 1); if (m_selectedInstalled != prev) AudioManager::get().playSound(SoundId::Navigate); }

    if (input.a) {
        // Open detail screen - try to find matching mod in repo
        auto& inst = m_installedMods[m_selectedInstalled];
        std::lock_guard<std::mutex> lock(m_repoMutex);
        bool found = false;
        for (auto& game : m_repo.games) {
            for (auto& mod : game.mods) {
                if (mod.id == inst.id) {
                    m_app->pushScreen(std::make_unique<DetailScreen>(
                        m_app, mod, game.name, game.titleIds));
                    found = true;
                    break;
                }
            }
            if (found) break;
        }
        if (!found) {
            // Fallback: build minimal Mod from InstalledMod
            Mod fallback;
            fallback.id      = inst.id;
            fallback.name    = inst.name.empty() ? inst.id : inst.name;
            fallback.version = inst.version;
            fallback.author  = "Unknown";
            fallback.description = "This mod was not found in any configured repository.";
            m_app->pushScreen(std::make_unique<DetailScreen>(
                m_app, fallback, "Unknown Game", std::vector<std::string>{inst.titleId}));
        }
    }
    if (input.x) {
        // Toggle active/inactive
        auto& mod = m_installedMods[m_selectedInstalled];
        if (!mod.active) {
            auto conflict = ConflictChecker::check(mod, m_installedMods);
            if (conflict.hasConflict) {
                m_conflictResult = conflict;
                m_showConflict   = true; AudioManager::get().playSound(SoundId::ConflictWarning);
            } else {
                InstalledScanner::setActive(mod, true);
                AudioManager::get().playSound(SoundId::ModActivated);
                m_installedDirty = true;
            }
        } else {
            InstalledScanner::setActive(mod, false);
            AudioManager::get().playSound(SoundId::ModDeactivated);
            m_installedDirty = true;
        }
    }
    // ZR -> "Update all" confirmation
    if (input.zr) {
        int updateCount = 0;
        for (auto& m : m_installedMods)
            if (InstalledScanner::hasUpdate(m)) updateCount++;
        if (updateCount > 0) {
            m_confirmUpdateAll = true;
            AudioManager::get().playSound(SoundId::Navigate);
        }
    }
    if (input.y) {
        m_confirmUninstall = true;
    }
}

void MainLayout::update() {
    // Audio startup logic - runs once when fetch completes
    if (!m_audioStarted) {
        if (m_fetchState == FetchState::Done) {
            m_audioStarted = true;
            AudioManager::get().playSound(SoundId::Startup);
            // Fade in music after a short delay via Mix_FadeInMusic
            // We use a simple frame counter to delay
            m_musicFadeDelay = 90; // ~3 seconds at 30fps
        } else if (m_fetchState == FetchState::Error) {
            m_audioStarted = true;
            AudioManager::get().playSound(SoundId::Error);
        }
    }
    if (m_fadeInAlpha > 0) m_fadeInAlpha = std::max(0, m_fadeInAlpha - 4); // ~64 frames
    if (m_musicFadeDelay > 0) {
        m_musicFadeDelay--;
        if (m_musicFadeDelay == 0 && m_config.musicTrack != "off") {
            // Route through AudioManager so the Mix_Music* gets tracked and
            // freed on shutdown (was leaked before).
            MusicTrack t = (m_config.musicTrack == "alt")
                ? MusicTrack::Alt : MusicTrack::Main;
            AudioManager::get().playMusicFadeIn(t, 3000);
        }
    }
    // Detect mutations from other screens (e.g. DetailScreen uninstall) by
    // comparing the InstalledScanner generation counter. This avoids stale
    // "ON" badges on the Browse cards when the cache was invalidated outside
    // our own action paths.
    unsigned gen = InstalledScanner::generation();
    if (gen != m_lastInstalledGen) {
        m_lastInstalledGen = gen;
        m_installedDirty   = true;
    }
    if (m_installedDirty)
        refreshInstalled();
    if (m_settingsDirty) {
        m_settingsItems = buildSettingsItems(m_config, m_repoStatus);
        m_settingsDirty = false;
    }
}

void MainLayout::render(SDL_Renderer* renderer) {
    static int s_mlCount = 0;
    bool diag = (s_mlCount < 3);
    if (diag) {
        elogf("ML::render #%d -- showOnboarding=%d activeTab=%d games=%zu fonts(N=%p,S=%p,T=%p)",
              s_mlCount, (int)m_showOnboarding, (int)m_activeTab,
              m_repo.games.size(),
              (void*)m_fontNormal, (void*)m_fontSmall, (void*)m_fontTiny);
    }
    s_mlCount++;
    if (m_showStartupConflicts && !m_startupConflicts.empty()) {
        const int W = m_app->screenWidth();
        const int H = m_app->screenHeight();
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 200);
        SDL_Rect fb = {0,0,W,H}; SDL_RenderFillRect(renderer, &fb);
        SDL_SetRenderDrawColor(renderer, 60, 40, 10, 255);
        SDL_Rect card = {W/2-280, H/2-120, 560, 240};
        SDL_RenderFillRect(renderer, &card);
        SDL_SetRenderDrawColor(renderer, 200, 140, 30, 255);
        SDL_RenderDrawRect(renderer, &card);
        if (m_fontSmall) {
            renderText(renderer, "Conflict Warning", W/2-240, H/2-104, {220,180,50,255}, m_fontSmall);
            renderText(renderer, "Active mods with file conflicts detected:", W/2-240, H/2-78, {200,200,220,255}, m_fontTiny ? m_fontTiny : m_fontSmall);
            int ry = H/2-58;
            for (auto& sc : m_startupConflicts) {
                std::string line = sc.modName + " conflicts with: ";
                for (size_t i = 0; i < sc.conflicts.size(); i++) {
                    if (i) line += ", ";
                    line += sc.conflicts[i];
                }
                if (m_fontTiny) renderText(renderer, line, W/2-240, ry, {200,160,100,255}, m_fontTiny);
                ry += 18;
                if (ry > H/2+80) break;
            }
            renderText(renderer, "A / B: Dismiss", W/2-240, H/2+90, {120,120,150,255}, m_fontTiny ? m_fontTiny : m_fontSmall);
        }
        return;
    }
    if (m_showOnboarding) { renderOnboarding(renderer); return; }
    renderSidebar(renderer);
    switch (m_activeTab) {
        case Tab::Browse:    renderBrowse(renderer);    break;
        case Tab::Installed: renderInstalled(renderer); break;
        case Tab::Settings:  renderSettings(renderer);  break;
    }
}

void MainLayout::renderSidebar(SDL_Renderer* renderer) {
    const int H = m_app->screenHeight();

    SDL_SetRenderDrawColor(renderer, 22, 22, 35, 255);
    SDL_Rect sidebar = {0, 0, SIDEBAR_W, H};
    SDL_RenderFillRect(renderer, &sidebar);
    SDL_SetRenderDrawColor(renderer, 50, 50, 70, 255);
    SDL_RenderDrawLine(renderer, SIDEBAR_W, 0, SIDEBAR_W, H);

    if (m_fontNormal)
        renderText(renderer, "CoffeeShop", 20, 18, {255,255,255,255}, m_fontNormal);

    SDL_SetRenderDrawColor(renderer, 50, 50, 70, 255);
    SDL_RenderDrawLine(renderer, 15, 58, SIDEBAR_W-15, 58);

    struct TabEntry { const char* label; Tab tab; };
    TabEntry tabs[] = {
        {"Browse",    Tab::Browse},
        {"Installed", Tab::Installed},
        {"Settings",  Tab::Settings},
    };
    int tabY = 75;
    for (auto& t : tabs) {
        bool active = (m_activeTab == t.tab);
        if (active) {
            SDL_SetRenderDrawColor(renderer, 40, 40, 65, 255);
            SDL_Rect bg = {5, tabY-4, SIDEBAR_W-10, 36};
            SDL_RenderFillRect(renderer, &bg);
            SDL_SetRenderDrawColor(renderer, 80, 180, 255, 255);
            SDL_Rect acc = {5, tabY-4, 4, 36};
            SDL_RenderFillRect(renderer, &acc);
        }
        SDL_Color color = active ? SDL_Color{80,180,255,255} : SDL_Color{160,160,180,255};
        if (m_fontNormal) renderText(renderer, t.label, 22, tabY, color, m_fontNormal);
        tabY += 52;
    }

    // Download badge
    int active = DownloadQueue::get().activeCount();
    if (active > 0 && m_fontTiny) {
        SDL_SetRenderDrawColor(renderer, 220, 70, 70, 255);
        SDL_Rect dot = {SIDEBAR_W-22, 12, 18, 18};
        SDL_RenderFillRect(renderer, &dot);
        renderText(renderer, std::to_string(active), SIDEBAR_W-17, 14, {255,255,255,255}, m_fontTiny);
    }

    SDL_Color grey = {80, 80, 105, 255};
    if (m_fontTiny) {
        renderText(renderer, "Minus: exit",       12, H-62, grey, m_fontTiny);
        renderText(renderer, "+: downloads",    12, H-26, grey, m_fontTiny);
        renderText(renderer, "L/R: switch tab", 12, H-44, grey, m_fontTiny);
    }
}

void MainLayout::renderBrowse(SDL_Renderer* renderer) {
    const int W = m_app->screenWidth();
    const int H = m_app->screenHeight();
    const int cx = SIDEBAR_W + 10;

    SDL_SetRenderDrawColor(renderer, 15, 15, 25, 255);
    SDL_Rect bg = {SIDEBAR_W, 0, W-SIDEBAR_W, H};
    SDL_RenderFillRect(renderer, &bg);

    std::lock_guard<std::mutex> lock(m_repoMutex);

    if (m_fetchState == FetchState::Loading) {
        if (m_fontNormal) renderText(renderer, "Loading repository...", cx+20, 60, {150,150,170,255}, m_fontNormal);
        if (m_fontSmall && !m_fetchProgress.empty())
            renderText(renderer, m_fetchProgress, cx+20, 95, {110,140,170,255}, m_fontSmall);
        return;
    }
    if (m_fetchState == FetchState::Error) {
        if (m_fontNormal) renderText(renderer, "Failed to load repository.", cx+20, 60, {220,70,70,255}, m_fontNormal);
        if (m_fontSmall && !m_fetchError.empty()) renderText(renderer, m_fetchError, cx+20, 96, {180,100,100,255}, m_fontSmall);
        return;
    }

    auto& games = m_repo.games;
    if (games.empty()) {
        if (m_fontNormal)
            renderText(renderer, "No mods in this repo.", cx+20, 80, {180,180,200,255}, m_fontNormal);
        if (m_fontSmall) {
            renderText(renderer, "Either the repo is empty or every game was filtered out.",
                       cx+20, 116, {130,130,160,255}, m_fontSmall);
            renderText(renderer, "Add another repo URL in config.json (Settings tab),",
                       cx+20, 138, {130,130,160,255}, m_fontSmall);
            renderText(renderer, "or check the repo URL is reachable.",
                       cx+20, 158, {130,130,160,255}, m_fontSmall);
        }
        return;
    }

    // Game filter pills - installed games first, with primary color
    // Build sorted game indices: installed first, then rest
    std::vector<int> gameOrder;
    std::vector<int> installedGameIndices;
    std::vector<int> otherGameIndices;
    for (int i = 0; i < (int)games.size(); i++) {
        bool hasInstalled = false;
        for (auto& inst : m_installedMods) {
            for (auto& tid : games[i].titleIds) {
                if (inst.titleId == tid) { hasInstalled = true; break; }
            }
            if (hasInstalled) break;
        }
        if (hasInstalled) installedGameIndices.push_back(i);
        else              otherGameIndices.push_back(i);
    }
    for (int i : installedGameIndices) gameOrder.push_back(i);
    for (int i : otherGameIndices)     gameOrder.push_back(i);

    int gx = cx + 10;
    for (int oi = 0; oi < (int)gameOrder.size(); oi++) {
        int i = gameOrder[oi];
        bool sel = (i == m_selectedGame);
        bool hasInstalled = std::find(installedGameIndices.begin(),
                                      installedGameIndices.end(), i) != installedGameIndices.end();

        // Icon
        int iconW = 0;
        SDL_Texture* iconTex = nullptr;
        if (!games[i].icon.empty()) {
            ImageCache::get().request(games[i].icon);
            iconTex = ImageCache::get().texture(games[i].icon, renderer);
            if (iconTex) iconW = 24;
        }

        // Count installed mods for this game's titleIds
        int installedHere = 0;
        for (auto& inst : m_installedMods) {
            for (auto& tid : games[i].titleIds) {
                if (inst.titleId == tid) { installedHere++; break; }
            }
        }
        std::string label = games[i].name;
        if (installedHere > 0)
            label += " (" + std::to_string(installedHere) + ")";

        int tw = 0;
        if (m_fontSmall) TTF_SizeText(m_fontSmall, label.c_str(), &tw, nullptr);
        int pillW = tw + 20 + iconW;

        // Colors: installed=blue-primary, other=dark-secondary
        SDL_Color bgc = sel
            ? SDL_Color{60, 140, 255, 255}
            : hasInstalled
                ? SDL_Color{30, 70, 140, 255}
                : SDL_Color{28, 28, 45, 255};
        SDL_Color borderC = sel
            ? SDL_Color{100, 180, 255, 255}
            : hasInstalled
                ? SDL_Color{50, 100, 180, 255}
                : SDL_Color{45, 45, 70, 255};

        SDL_Rect pill = {gx, 8, pillW, 30};
        SDL_SetRenderDrawColor(renderer, bgc.r, bgc.g, bgc.b, 255);
        SDL_RenderFillRect(renderer, &pill);
        SDL_SetRenderDrawColor(renderer, borderC.r, borderC.g, borderC.b, 255);
        SDL_RenderDrawRect(renderer, &pill);

        // Icon rendering
        int textX = gx + 10;
        if (iconTex) {
            SDL_Rect iconRect = {gx + 5, 11, 22, 22};
            SDL_RenderCopy(renderer, iconTex, nullptr, &iconRect);
            textX = gx + 32;
        }

        if (m_fontSmall)
            renderText(renderer, label, textX, 14,
                       sel ? SDL_Color{255,255,255,255} : SDL_Color{180,180,210,255}, m_fontSmall);

        gx += pillW + 8;
    }

    // Cards - filtered + sorted copy.
    // 1) tag filter: if any tags are selected, show only mods with at least
    //    one matching tag.
    std::vector<Mod> sortedMods;
    sortedMods.reserve(games[m_selectedGame].mods.size());
    for (auto& m : games[m_selectedGame].mods) {
        if (m_activeTags.empty()) {
            sortedMods.push_back(m);
        } else {
            for (auto& t : m.tags) {
                if (m_activeTags.count(t)) {
                    sortedMods.push_back(m);
                    break;
                }
            }
        }
    }
    // 2) sort
    switch (m_sortMode) {
        case SortMode::NameAZ:
            std::sort(sortedMods.begin(), sortedMods.end(),
                [](const Mod& a, const Mod& b){ return a.name < b.name; });
            break;
        case SortMode::Version:
            std::sort(sortedMods.begin(), sortedMods.end(),
                [](const Mod& a, const Mod& b){ return a.version > b.version; });
            break;
        default: break;
    }
    auto& mods = sortedMods;
    // Skip cards above the visible scroll window or below the bottom hint
    // line. Without this, render walks through hundreds of cards offscreen
    // for large repos, and the cursor visually disappears past the screen.
    const int rowH = CARD_H + CARD_PAD;
    const int bottomHint = 30;
    for (int i = 0; i < (int)mods.size(); i++) {
        auto& mod = mods[i];
        bool  sel = (i == m_selectedMod);
        int   row = i / CARDS_PER_ROW;
        if (row < m_browseScrollRow) continue;          // above viewport
        int   x   = cx + CARD_PAD + (i % CARDS_PER_ROW) * (CARD_W + CARD_PAD);
        int   y   = GRID_TOP      + (row - m_browseScrollRow) * rowH;
        if (y + CARD_H > H - bottomHint) break;          // below viewport (rows are in order)

        SDL_SetRenderDrawColor(renderer, 25, 25, 40, 255);
        SDL_Rect card = {x, y, CARD_W, CARD_H};
        SDL_RenderFillRect(renderer, &card);
        SDL_SetRenderDrawColor(renderer, sel ? 80 : 45, sel ? 180 : 45, sel ? 255 : 65, 255);
        SDL_RenderDrawRect(renderer, &card);

        SDL_Rect thumb = {x+4, y+4, CARD_W-8, 90};
        SDL_Texture* thumbTex = nullptr;
        if (!mod.thumbnail.empty()) {
            ImageCache::get().request(mod.thumbnail);
            thumbTex = ImageCache::get().texture(mod.thumbnail, renderer);
        }
        if (thumbTex) {
            // Aspect-correct crop: scale to fill width, crop bottom
            int tw = 0, th = 0;
            SDL_QueryTexture(thumbTex, nullptr, nullptr, &tw, &th);
            int thumbH = thumb.h; // target height
            int thumbW = thumb.w; // target width
            // Source rect: full width, crop height to match aspect
            int srcH = (th * thumbW) / tw; // how tall src would be at target width
            SDL_Rect src;
            if (srcH <= thumbH) {
                // Image is wider than target - use full height, crop width
                src = {0, 0, tw, th};
            } else {
                // Image is taller - crop from top
                src = {0, 0, tw, (tw * thumbH) / thumbW};
            }
            SDL_RenderCopy(renderer, thumbTex, &src, &thumb);
        } else {
            SDL_SetRenderDrawColor(renderer, 32, 32, 52, 255);
            SDL_RenderFillRect(renderer, &thumb);
            SDL_SetRenderDrawColor(renderer, 50, 50, 78, 255);
            SDL_RenderDrawLine(renderer, x+CARD_W/2-14, y+49, x+CARD_W/2+14, y+49);
            SDL_RenderDrawLine(renderer, x+CARD_W/2, y+35, x+CARD_W/2, y+63);
        }

        bool isMp = (mod.type == "modpack");
        SDL_Color bb = isMp ? SDL_Color{120,60,180,255} : SDL_Color{40,100,180,255};
        SDL_Rect badge = {x+CARD_W-(isMp?82:46), y+6, isMp?76:40, 20};
        SDL_SetRenderDrawColor(renderer, bb.r, bb.g, bb.b, 255);
        SDL_RenderFillRect(renderer, &badge);
        if (m_fontTiny) renderText(renderer, isMp?"MODPACK":"MOD", badge.x+5, badge.y+3, {255,255,255,255}, m_fontTiny);

        // Update badge - check if installed version differs from repo version
        for (auto& inst : m_installedMods) {
            if (inst.id == mod.id && inst.titleId.size() > 0) {
                if (!inst.version.empty() && inst.version != mod.version) {
                    SDL_SetRenderDrawColor(renderer, 180, 110, 20, 255);
                    SDL_Rect upBadge = {x+6, y+6, 62, 20};
                    SDL_RenderFillRect(renderer, &upBadge);
                    if (m_fontTiny) renderText(renderer, "UPDATE", upBadge.x+5, upBadge.y+3, {255,255,255,255}, m_fontTiny);
                } else {
                    // Installed indicator (small dot)
                    SDL_SetRenderDrawColor(renderer, 40, 160, 80, 255);
                    SDL_Rect dot = {x+6, y+6, 36, 20};
                    SDL_RenderFillRect(renderer, &dot);
                    if (m_fontTiny) renderText(renderer, "ON", dot.x+8, dot.y+3, {255,255,255,255}, m_fontTiny);
                }
                break;
            }
        }

        if (m_fontSmall) renderText(renderer, mod.name, x+8, y+100, {220,220,240,255}, m_fontSmall);
        if (m_fontTiny) { std::string av = mod.author+" v"+mod.version; if ((int)av.size() > 30) av = av.substr(0,27)+"..."; renderText(renderer, av, x+8, y+124, {110,110,140,255}, m_fontTiny); }
    }

    if (m_fontTiny) {
        const char* sortLabel = m_sortMode == SortMode::NameAZ ? "Sort: Name A-Z"
                              : m_sortMode == SortMode::Version ? "Sort: Version"
                              : "Sort: Default";
        renderText(renderer, "D-Pad   A: details   L/R: game   X: sort   Y: filter   +: downloads",
                   cx+10, H-22, {70,70,95,255}, m_fontTiny);
        std::string status = sortLabel;
        if (!m_activeTags.empty())
            status = std::string("Filter: ") + std::to_string((int)m_activeTags.size()) + " tag(s) | " + sortLabel;
        renderText(renderer, status, W-260, H-22, {100,160,100,255}, m_fontTiny);
    }
}

void MainLayout::renderInstalled(SDL_Renderer* renderer) {
    const int W = m_app->screenWidth();
    const int H = m_app->screenHeight();
    const int cx = SIDEBAR_W + 20;

    SDL_SetRenderDrawColor(renderer, 15, 15, 25, 255);
    SDL_Rect bg = {SIDEBAR_W, 0, W-SIDEBAR_W, H};
    SDL_RenderFillRect(renderer, &bg);

    if (m_installedMods.empty()) {
        if (m_fontNormal)
            renderText(renderer, "Nothing installed yet.", cx, 80, {180,180,200,255}, m_fontNormal);
        if (m_fontSmall) {
            renderText(renderer, "Press L to switch to Browse and install your first mod.",
                       cx, 116, {130,130,160,255}, m_fontSmall);
        }
        return;
    }

    int y = 16;
    for (int i = 0; i < (int)m_installedMods.size(); i++) {
        auto& mod = m_installedMods[i];
        bool  sel = (i == m_selectedInstalled);

        SDL_Color rowBg = sel ? SDL_Color{30,30,55,255} : SDL_Color{20,20,35,255};
        SDL_SetRenderDrawColor(renderer, rowBg.r, rowBg.g, rowBg.b, 255);
        SDL_Rect row = {SIDEBAR_W+4, y, W-SIDEBAR_W-8, 52};
        SDL_RenderFillRect(renderer, &row);

        if (sel) {
            SDL_SetRenderDrawColor(renderer, 80, 180, 255, 255);
            SDL_Rect acc = {SIDEBAR_W+4, y, 3, 52};
            SDL_RenderFillRect(renderer, &acc);
        }

        // Active/inactive indicator
        SDL_Color stateColor = mod.active ? SDL_Color{55,190,95,255} : SDL_Color{130,130,150,255};
        std::string stateLabel = mod.active ? "Active" : "Inactive";
        if (m_fontTiny) renderText(renderer, stateLabel, W-110, y+8, stateColor, m_fontTiny);

        // Update badge
        if (InstalledScanner::hasUpdate(mod) && m_fontTiny) {
            SDL_SetRenderDrawColor(renderer, 220, 140, 30, 255);
            SDL_Rect badge = {W-170, y+6, 52, 18};
            SDL_RenderFillRect(renderer, &badge);
            renderText(renderer, "UPDATE", W-167, y+8, {255,255,255,255}, m_fontTiny);
        }

        // Name
        std::string displayName = mod.name.empty() ? mod.id : mod.name;
        if (m_fontSmall) renderText(renderer, displayName, cx, y+6, {220,220,240,255}, m_fontSmall);

        // Version + titleId
        if (m_fontTiny) {
            std::string sub = "v" + mod.version + "  " + mod.titleId;
            renderText(renderer, sub, cx, y+30, {100,100,130,255}, m_fontTiny);
        }

        y += 58;
        if (y > H - 50) break;
    }

    // Bottom hints
    if (m_fontTiny) {
        SDL_Color grey = {70, 70, 95, 255};
        int updateCount = 0;
        for (auto& m : m_installedMods)
            if (InstalledScanner::hasUpdate(m)) updateCount++;
        std::string hint = "A: details   X: toggle   Y: uninstall";
        if (updateCount > 0)
            hint += "   ZR: update all (" + std::to_string(updateCount) + ")";
        renderText(renderer, hint, cx, H-22, grey, m_fontTiny);
    }

    // Conflict warning overlay
    if (m_showConflict) {
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 210);
        SDL_Rect overlay = {0, 0, W, H};
        SDL_RenderFillRect(renderer, &overlay);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

        SDL_SetRenderDrawColor(renderer, 30, 25, 15, 255);
        SDL_Rect card = {W/2-280, H/2-120, 560, 240};
        SDL_RenderFillRect(renderer, &card);
        SDL_SetRenderDrawColor(renderer, 200, 140, 30, 255);
        SDL_RenderDrawRect(renderer, &card);

        if (m_fontSmall) {
            renderText(renderer, "Conflict Warning", W/2-255, H/2-108, {255,200,50,255}, m_fontSmall);
            // Render conflicting mods, one per line
            renderText(renderer, "Conflicts with:", W/2-255, H/2-80, {220,200,150,255}, m_fontSmall);
            int mody = H/2-58;
            for (size_t i = 0; i < m_conflictResult.conflictingMods.size() && i < 3; i++) {
                renderText(renderer, "  - " + m_conflictResult.conflictingMods[i], W/2-240, mody, {220,200,150,255}, m_fontSmall);
                mody += 20;
            }
            if (!m_conflictResult.conflictingFiles.empty()) {
                renderText(renderer, "Conflicting files:", W/2-255, H/2+10, {150,140,110,255}, m_fontSmall);
                int fy = H/2+30;
                for (auto& f : m_conflictResult.conflictingFiles) {
                    if (m_fontTiny) renderText(renderer, "  " + f, W/2-255, fy, {130,120,100,255}, m_fontTiny);
                    fy += 18;
                    if (fy > H/2+80) break;
                }
            }
        }
        if (m_fontNormal) {
            renderText(renderer, "A: Activate anyway", W/2-255, H/2+50, {200,140,30,255}, m_fontSmall);
            renderText(renderer, "B: Cancel",          W/2+50,  H/2+50, {130,130,160,255}, m_fontSmall);
        }
    }

    // Confirm uninstall overlay
    if (m_confirmUninstall && !m_installedMods.empty()) {
        auto& mod = m_installedMods[m_selectedInstalled];
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 180);
        SDL_Rect overlay = {0, 0, W, H};
        SDL_RenderFillRect(renderer, &overlay);

        SDL_SetRenderDrawColor(renderer, 30, 20, 20, 255);
        SDL_Rect card = {W/2-220, H/2-70, 440, 140};
        SDL_RenderFillRect(renderer, &card);
        SDL_SetRenderDrawColor(renderer, 180, 60, 60, 255);
        SDL_RenderDrawRect(renderer, &card);

        if (m_fontSmall) {
            std::string msg = "Uninstall '" + (mod.name.empty() ? mod.id : mod.name) + "'?";
            renderText(renderer, msg,               W/2-180, H/2-50, {255,255,255,255}, m_fontSmall);
            renderText(renderer, "This deletes the mod folder permanently.", W/2-180, H/2-22, {180,130,130,255}, m_fontSmall);
        }
        if (m_fontNormal) {
            renderText(renderer, "A: Confirm",  W/2-140, H/2+16, {220,70,70,255},   m_fontNormal);
            renderText(renderer, "B: Cancel",   W/2+20,  H/2+16, {130,130,160,255}, m_fontNormal);
        }
    }

    // Confirm update-all overlay
    if (m_confirmUpdateAll) {
        int updateCount = 0;
        for (auto& m : m_installedMods)
            if (InstalledScanner::hasUpdate(m)) updateCount++;

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 180);
        SDL_Rect overlay = {0, 0, W, H};
        SDL_RenderFillRect(renderer, &overlay);

        SDL_SetRenderDrawColor(renderer, 18, 28, 38, 255);
        SDL_Rect card = {W/2-240, H/2-80, 480, 160};
        SDL_RenderFillRect(renderer, &card);
        SDL_SetRenderDrawColor(renderer, 80, 180, 255, 255);
        SDL_RenderDrawRect(renderer, &card);

        if (m_fontNormal) {
            std::string title = "Update " + std::to_string(updateCount) +
                                (updateCount == 1 ? " mod?" : " mods?");
            renderText(renderer, title, W/2-200, H/2-58, {255,255,255,255}, m_fontNormal);
        }
        if (m_fontSmall) {
            renderText(renderer, "All mods with available updates will be enqueued.",
                       W/2-200, H/2-24, {180,200,220,255}, m_fontSmall);
        }
        if (m_fontNormal) {
            renderText(renderer, "A: Update all", W/2-180, H/2+24, {80,180,255,255},   m_fontNormal);
            renderText(renderer, "B: Cancel",     W/2+40,  H/2+24, {130,130,160,255},  m_fontNormal);
        }
    }
}



std::vector<SItem> MainLayout::buildSettingsItems(const Config& cfg,
    const std::map<std::string, std::string>& repoStatus) {
    std::vector<SItem> items;
    auto header = [&](const char* t)                          { items.push_back({SItemType::Header, t, ""}); };
    auto info   = [&](const char* l, const std::string& v)   { items.push_back({SItemType::Info,   l, v});  };
    auto button = [&](const char* l)                          { items.push_back({SItemType::Button, l, ""}); };

    header("Repos");
    for (auto& url : cfg.repos) {
        auto it = repoStatus.find(url);
        std::string status = it != repoStatus.end() ? it->second : "Pending...";
        // Truncate long URLs for display
        std::string shortUrl = url.size() > 50 ? url.substr(0, 47) + "..." : url;
        info(shortUrl.c_str(), status);
    }
    info("Config file", Paths::configFile());

    header("System");
    struct stat _st;
    info("SDCafiine folder", stat(Paths::sdcafiineBase().c_str(), &_st)==0 ? "Found" : "Not found");
    info("SD card",          stat(Paths::sdRoot().c_str(),        &_st)==0 ? "Mounted" : "Not detected");
    struct statvfs sv;
    std::string freeStr = "Unknown";
    if (statvfs(Paths::sdRoot().c_str(), &sv) == 0) {
        uint64_t fr = (uint64_t)sv.f_bavail * sv.f_frsize;
        char buf[32];
        if (fr < 1024*1024) snprintf(buf,sizeof(buf),"%.1f KB",fr/1024.0);
        else snprintf(buf,sizeof(buf),"%.1f MB",fr/(1024.0*1024.0));
        freeStr = buf;
    }
    info("Free space", freeStr);
    auto mods = InstalledScanner::scan();
    int act=0, inact=0;
    for (auto& m : mods) { if(m.active) act++; else inact++; }
    info("Installed mods", std::to_string(act)+" active / "+std::to_string(inact)+" inactive");

    header("Cache");
    // Compute cache sizes
    auto szDir = [](const std::string& p) -> uint64_t {
        uint64_t t=0; DIR* d=opendir(p.c_str()); if(!d) return 0;
        struct dirent* e;
        while((e=readdir(d))!=nullptr) {
            std::string n=e->d_name; if(n=="."||n=="..") continue;
            struct stat s; std::string c=p+"/"+n;
            if(stat(c.c_str(),&s)==0 && !S_ISDIR(s.st_mode)) t+=s.st_size;
        }
        closedir(d); return t;
    };
    auto fmtSz=[](uint64_t b)->std::string{
        char buf[32];
        if(b<1024) snprintf(buf,sizeof(buf),"%llu B",(unsigned long long)b);
        else if(b<1024*1024) snprintf(buf,sizeof(buf),"%.1f KB",b/1024.0);
        else snprintf(buf,sizeof(buf),"%.1f MB",b/(1024.0*1024.0));
        return buf;
    };
    info("Image cache", fmtSz(szDir(Paths::cacheDir()+"/images")));
    info("Total cache",  fmtSz(szDir(Paths::cacheDir())));
    button("Clear image cache");
    button("Clear all cache");
    button("Find corrupt mod folders");

    header("Logs");
    info("Log file", Logger::get().path().empty() ? "Not initialized" : Logger::get().path());
    button("View log");

    header("App");
#ifdef APP_VERSION
    info("Version", APP_VERSION);
#else
    info("Version", "unknown");
#endif
#if BUILD_HW
    info("Build", "Hardware");
#else
    info("Build", "Cemu / Debug");
#endif
    info("Author", "Tim Kicker");
    info("GitHub", "github.com/timkicker/coffeeshop");

    return items;
}

void MainLayout::handleSettingsInput(const Input& input) {
    // Log viewer
    if (m_showLog) {
        auto lines = Logger::get().lines();
        if (input.up)   m_logScroll = std::max(0, m_logScroll - 1);
        if (input.down) m_logScroll = std::min(std::max(0,(int)lines.size()-20), m_logScroll+1);
        if (input.b)    m_showLog = false;
        return;
    }

    auto& items = m_settingsItems;

    auto selectable = [&](int i) {
        return i >= 0 && i < (int)items.size() && items[i].type != SItemType::Header;
    };

    if (input.up) {
        int n = m_settingsSelected - 1;
        while (n >= 0 && !selectable(n)) n--;
        if (n >= 0) m_settingsSelected = n;
    }
    if (input.down) {
        int n = m_settingsSelected + 1;
        while (n < (int)items.size() && !selectable(n)) n++;
        if (n < (int)items.size()) m_settingsSelected = n;
    }

    // Scroll
    if (m_settingsSelected < m_settingsScroll) m_settingsScroll = m_settingsSelected;
    if (m_settingsSelected >= m_settingsScroll + 16) m_settingsScroll = m_settingsSelected - 15;

    if (input.a && selectable(m_settingsSelected) &&
        items[m_settingsSelected].type == SItemType::Button) {
        std::string label = items[m_settingsSelected].label;

        if (label == "Clear image cache") {
            std::string imgDir = Paths::cacheDir() + "/images";
            DIR* d = opendir(imgDir.c_str());
            if (d) { struct dirent* e;
                while((e=readdir(d))!=nullptr) {
                    std::string n=e->d_name; if(n=="."||n=="..") continue;
                    remove((imgDir+"/"+n).c_str());
                } closedir(d);
            }
            ImageCache::get().clear(nullptr);
            LOG_INFO("Settings: image cache cleared");
            m_settingsDirty = true;
        }
        else if (label == "Clear all cache") {
            std::string dir = Paths::cacheDir();
            DIR* d = opendir(dir.c_str());
            if (d) { struct dirent* e;
                while((e=readdir(d))!=nullptr) {
                    std::string n=e->d_name; if(n=="."||n=="..") continue;
                    remove((dir+"/"+n).c_str());
                } closedir(d);
            }
            ImageCache::get().clear(nullptr);
            LOG_INFO("Settings: full cache cleared");
            m_settingsDirty = true;
        }
        else if (label == "Find corrupt mod folders") {
            // Scan-only first: list folders without modinfo.json so the user
            // can confirm before any delete happens. Replaces the old
            // boot-time auto-cleanup that was hostile to manually-installed
            // mods and hung on hardware (large rmrf).
            std::vector<std::string> corrupt;
            std::vector<std::string> bases = { Paths::sdcafiineBase(), Paths::disabledBase() };
            for (auto& base : bases) {
                DIR* td = opendir(base.c_str());
                if (!td) continue;
                struct dirent* te;
                while ((te = readdir(td)) != nullptr) {
                    std::string tid = te->d_name;
                    if (tid == "." || tid == "..") continue;
                    std::string tpath = base + "/" + tid;
                    DIR* md = opendir(tpath.c_str());
                    if (!md) continue;
                    struct dirent* me;
                    while ((me = readdir(md)) != nullptr) {
                        std::string mid = me->d_name;
                        if (mid == "." || mid == "..") continue;
                        std::string mpath = tpath + "/" + mid;
                        struct stat st;
                        if (stat((mpath + "/modinfo.json").c_str(), &st) != 0)
                            corrupt.push_back(tid + "/" + mid);
                    }
                    closedir(md);
                }
                closedir(td);
            }
            // Show count to user as a transient log line. A future iteration
            // can push a dedicated screen with per-folder confirmation.
            LOG_INFO("Settings: found %zu folders without modinfo.json", corrupt.size());
            for (auto& c : corrupt) LOG_INFO("  candidate: %s", c.c_str());
            // For now: no delete. User reviews via "View log".
        }
        else if (label == "View log") {
            m_showLog   = true;
            m_logScroll = std::max(0,(int)Logger::get().lines().size()-20);
        }
    }
}

void MainLayout::renderSettings(SDL_Renderer* renderer) {
    const int W  = m_app->screenWidth();
    const int H  = m_app->screenHeight();
    const int CX = SIDEBAR_W + 20;
    const int VW = W - SIDEBAR_W - 40;

    SDL_SetRenderDrawColor(renderer, 15, 15, 25, 255);
    SDL_Rect bg = {SIDEBAR_W, 0, W-SIDEBAR_W, H};
    SDL_RenderFillRect(renderer, &bg);

    // Log overlay
    if (m_showLog) {
        SDL_SetRenderDrawColor(renderer, 8, 8, 14, 245);
        SDL_RenderFillRect(renderer, &bg);
        if (m_fontSmall) renderText(renderer, "Log Viewer   B: Close   Up/Down: scroll", CX, 14, {80,180,255,255}, m_fontSmall);
        SDL_SetRenderDrawColor(renderer, 25, 25, 40, 255);
        SDL_Rect logBg = {SIDEBAR_W+10, 40, VW+10, H-50};
        SDL_RenderFillRect(renderer, &logBg);
        auto lines = Logger::get().lines();
        int vis = (H - 60) / 16;
        int y = 46;
        for (int i = m_logScroll; i < (int)lines.size() && i < m_logScroll+vis; i++) {
            SDL_Color col = {180,180,200,255};
            if (lines[i].find("ERROR") != std::string::npos) col = {220,70,70,255};
            else if (lines[i].find("WARN") != std::string::npos) col = {220,180,50,255};
            if (m_fontTiny) renderText(renderer, lines[i], CX, y, col, m_fontTiny);
            y += 16;
        }
        std::string pg = std::to_string(m_logScroll+1)+"/"+std::to_string(std::max(1,(int)lines.size()));
        if (m_fontTiny) renderText(renderer, pg, W-80, H-20, {80,80,110,255}, m_fontTiny);
        return;
    }

    auto& items = m_settingsItems;

    const int ITEM_H = 32;
    int y = 8;

    for (int i = m_settingsScroll; i < (int)items.size() && y < H-30; i++) {
        auto& item = items[i];
        bool selected = (i == m_settingsSelected);

        if (item.type == SItemType::Header) {
            y += 8;
            if (m_fontSmall) renderText(renderer, item.label, CX, y, {80,180,255,255}, m_fontSmall);
            y += 18;
            SDL_SetRenderDrawColor(renderer, 40, 40, 65, 255);
            SDL_RenderDrawLine(renderer, CX, y, CX+VW, y);
            y += 6;
            continue;
        }

        if (selected) {
            SDL_SetRenderDrawColor(renderer, 28, 48, 78, 255);
            SDL_Rect row = {SIDEBAR_W+6, y-2, W-SIDEBAR_W-12, ITEM_H-4};
            SDL_RenderFillRect(renderer, &row);
            SDL_SetRenderDrawColor(renderer, 80,180,255,255);
            SDL_RenderDrawRect(renderer, &row);
        }

        SDL_Color labelCol = item.type == SItemType::Button
            ? SDL_Color{100,200,120,255}
            : SDL_Color{210,210,230,255};

        if (m_fontSmall) renderText(renderer, item.label, CX+6, y+6, labelCol, m_fontSmall);
        if (!item.value.empty() && m_fontSmall)
            renderText(renderer, item.value, CX+VW/2, y+6, {130,130,160,255}, m_fontSmall);

        y += ITEM_H;
    }

    if (m_fontTiny) {
        bool onBtn = m_settingsSelected < (int)items.size() &&
                     items[m_settingsSelected].type == SItemType::Button;
        std::string hint = onBtn
            ? "A: Execute   B: Back   Up/Down: navigate   L/R: switch tab"
            : "B: Back   Up/Down: navigate   L/R: switch tab";
        renderText(renderer, hint, CX, H-22, {70,70,95,255}, m_fontTiny);
    }
}

void MainLayout::renderOnboarding(SDL_Renderer* renderer) {
    const int W = m_app->screenWidth();
    const int H = m_app->screenHeight();
    SDL_SetRenderDrawColor(renderer, 15, 15, 25, 255);
    SDL_Rect bg = {0, 0, W, H};
    SDL_RenderFillRect(renderer, &bg);
    SDL_SetRenderDrawColor(renderer, 25, 25, 42, 255);
    SDL_Rect card = {W/2-260, H/2-120, 520, 240};
    SDL_RenderFillRect(renderer, &card);
    SDL_SetRenderDrawColor(renderer, 50, 50, 75, 255);
    SDL_RenderDrawRect(renderer, &card);
    if (m_fontNormal)
        renderText(renderer, "No repository configured.", W/2-180, H/2-90, {255,255,255,255}, m_fontNormal);
    if (m_fontSmall) {
        renderText(renderer, "Edit config.json and add a repo URL:", W/2-200, H/2-48, {150,150,175,255}, m_fontSmall);
        renderText(renderer, "{ \"repos\": [\"https://...\"] }",        W/2-160, H/2-20, {100,180,255,255}, m_fontSmall);
        renderText(renderer, "A: Retry",                               W/2- 40, H/2+60, {100,100,130,255}, m_fontSmall);
    }
}

void MainLayout::renderText(SDL_Renderer* renderer, const std::string& text,
                             int x, int y, SDL_Color color, TTF_Font* font) {
    if (!font || text.empty()) return;
    SDL_Texture* t = TextCache::get().texture(renderer, font, color, text);
    if (!t) return;
    int w = 0, h = 0;
    TextCache::get().sizeOf(t, &w, &h);
    SDL_Rect dst = {x, y, w, h};
    SDL_RenderCopy(renderer, t, nullptr, &dst);
}
