#include "MainLayout.h"
#include "app/App.h"
#include "ui/DetailScreen.h"
#include "ui/DownloadQueueScreen.h"
#include "net/DownloadQueue.h"
#include "mods/InstalledScanner.h"
#include "util/Logger.h"
#include "util/ImageCache.h"

static constexpr const char* FONT_PATH = "/vol/content/Roboto-Regular.ttf";
static constexpr int CARDS_PER_ROW = 3;
static constexpr int CARD_W        = 280;
static constexpr int CARD_H        = 160;
static constexpr int CARD_PAD      = 20;
static constexpr int GRID_TOP      = 70;

MainLayout::MainLayout(App* app) : Screen(app) {}

MainLayout::~MainLayout() {
    if (m_fetchThread.joinable()) m_fetchThread.join();
    if (m_fontNormal) TTF_CloseFont(m_fontNormal);
    if (m_fontSmall)  TTF_CloseFont(m_fontSmall);
    if (m_fontTiny)   TTF_CloseFont(m_fontTiny);
}

void MainLayout::onEnter() {
    m_fontNormal = TTF_OpenFont(FONT_PATH, 26);
    m_fontSmall  = TTF_OpenFont(FONT_PATH, 18);
    m_fontTiny   = TTF_OpenFont(FONT_PATH, 14);
    m_config.load();
    m_showOnboarding = !m_config.hasRepos();
    if (!m_showOnboarding && !m_config.repos.empty()) {
        m_fetchState = FetchState::Loading;
        m_fetchThread = std::thread([this]() {
            Repo combined;
            std::string lastError;

            for (auto& url : m_config.repos) {
                RepoManager rm;
                rm.fetch(url);
                if (!rm.lastError().empty()) {
                    lastError = url + ": " + rm.lastError();
                    LOG_WARN("MainLayout: repo failed: %s", lastError.c_str());
                    continue;
                }
                // Merge games from this repo into combined
                for (auto& g : rm.repo().games)
                    combined.games.push_back(g);
            }

            std::lock_guard<std::mutex> lock(m_repoMutex);
            m_repo = combined;
            if (combined.games.empty()) {
                m_fetchError = lastError.empty() ? "No repos returned any mods" : lastError;
                m_fetchState = FetchState::Error;
            } else {
                m_fetchState = FetchState::Done;
            }
        });
    }
}

void MainLayout::onExit() {}

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
    if (m_showOnboarding) {
        if (input.a) { m_config.load(); m_showOnboarding = !m_config.hasRepos(); }
        return;
    }

    if (input.y) {
        m_app->pushScreen(std::make_unique<DownloadQueueScreen>(m_app));
        return;
    }

    if (input.l) {
        if      (m_activeTab == Tab::Installed) m_activeTab = Tab::Browse;
        else if (m_activeTab == Tab::Settings)  m_activeTab = Tab::Installed;
    }
    if (input.r) {
        if      (m_activeTab == Tab::Browse)    m_activeTab = Tab::Installed;
        else if (m_activeTab == Tab::Installed) m_activeTab = Tab::Settings;
    }

    if (m_activeTab == Tab::Browse)    handleBrowseInput(input);
    if (m_activeTab == Tab::Installed) handleInstalledInput(input);
}

void MainLayout::handleBrowseInput(const Input& input) {
    std::lock_guard<std::mutex> lock(m_repoMutex);
    auto& games = m_repo.games;
    if (games.empty()) return;

    auto& mods     = games[m_selectedGame].mods;
    int   modCount = (int)mods.size();
    int   cols     = CARDS_PER_ROW;

    if (input.right && (m_selectedMod % cols) < cols - 1 && m_selectedMod + 1 < modCount) m_selectedMod++;
    if (input.left  && (m_selectedMod % cols) > 0)                                          m_selectedMod--;
    if (input.down) { int n = m_selectedMod + cols; if (n < modCount) m_selectedMod = n; }
    if (input.up)   { int p = m_selectedMod - cols; if (p >= 0)       m_selectedMod = p; }

    if (input.a && !mods.empty()) {
        const auto& game = games[m_selectedGame];
        m_app->pushScreen(std::make_unique<DetailScreen>(
            m_app, game.mods[m_selectedMod], game.name, game.titleIds));
    }
}

void MainLayout::handleInstalledInput(const Input& input) {
    if (m_installedMods.empty()) return;

    // Conflict warning dialog
    if (m_showConflict) {
        if (input.a) {
            // Proceed anyway
            InstalledScanner::setActive(m_installedMods[m_selectedInstalled], true);
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

    if (input.up)   m_selectedInstalled = std::max(0, m_selectedInstalled - 1);
    if (input.down) m_selectedInstalled = std::min((int)m_installedMods.size()-1, m_selectedInstalled + 1);

    if (input.a) {
        auto& mod = m_installedMods[m_selectedInstalled];
        if (!mod.active) {
            // Activating - check for conflicts first
            auto conflict = ConflictChecker::check(mod, m_installedMods);
            if (conflict.hasConflict) {
                m_conflictResult = conflict;
                m_showConflict   = true;
            } else {
                InstalledScanner::setActive(mod, true);
                m_installedDirty = true;
            }
        } else {
            // Deactivating - no conflict check needed
            InstalledScanner::setActive(mod, false);
            m_installedDirty = true;
        }
    }
    if (input.y) {
        m_confirmUninstall = true;
    }
    if (input.x) {
        m_installedDirty = true;
    }
}

void MainLayout::update() {
    if (m_installedDirty)
        refreshInstalled();
}

void MainLayout::render(SDL_Renderer* renderer) {
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
        renderText(renderer, "Mod Store", 20, 18, {255,255,255,255}, m_fontNormal);

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
        renderText(renderer, "L/R: switch tab", 12, H-44, grey, m_fontTiny);
        renderText(renderer, "Y: downloads",    12, H-26, grey, m_fontTiny);
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
        return;
    }
    if (m_fetchState == FetchState::Error) {
        if (m_fontNormal) renderText(renderer, "Failed to load repository.", cx+20, 60, {220,70,70,255}, m_fontNormal);
        if (m_fontSmall && !m_fetchError.empty()) renderText(renderer, m_fetchError, cx+20, 96, {180,100,100,255}, m_fontSmall);
        return;
    }

    auto& games = m_repo.games;
    if (games.empty()) {
        if (m_fontNormal) renderText(renderer, "No mods found.", cx+20, 60, {150,150,170,255}, m_fontNormal);
        return;
    }

    // Game filter pills
    int gx = cx + 10;
    for (int i = 0; i < (int)games.size(); i++) {
        bool sel = (i == m_selectedGame);
        int tw = 0;
        if (m_fontSmall) TTF_SizeText(m_fontSmall, games[i].name.c_str(), &tw, nullptr);
        SDL_Color bgc = sel ? SDL_Color{50,120,220,255} : SDL_Color{30,30,50,255};
        SDL_Rect pill = {gx, 12, tw+20, 28};
        SDL_SetRenderDrawColor(renderer, bgc.r, bgc.g, bgc.b, 255);
        SDL_RenderFillRect(renderer, &pill);
        if (m_fontSmall)
            renderText(renderer, games[i].name, gx+10, 16,
                       sel ? SDL_Color{255,255,255,255} : SDL_Color{160,160,190,255}, m_fontSmall);
        gx += tw + 30;
    }

    // Cards
    auto& mods = games[m_selectedGame].mods;
    for (int i = 0; i < (int)mods.size(); i++) {
        auto& mod = mods[i];
        bool  sel = (i == m_selectedMod);
        int   x   = cx + CARD_PAD + (i % CARDS_PER_ROW) * (CARD_W + CARD_PAD);
        int   y   = GRID_TOP      + (i / CARDS_PER_ROW) * (CARD_H + CARD_PAD);

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
            SDL_RenderCopy(renderer, thumbTex, nullptr, &thumb);
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
        if (m_fontTiny)  renderText(renderer, mod.author+" v"+mod.version, x+8, y+124, {110,110,140,255}, m_fontTiny);
    }

    if (m_fontTiny)
        renderText(renderer, "D-Pad: navigate   A: details   Y: downloads",
                   cx+10, H-22, {70,70,95,255}, m_fontTiny);
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
            renderText(renderer, "No mods installed.", cx, 60, {150,150,170,255}, m_fontNormal);
        if (m_fontTiny)
            renderText(renderer, "X: refresh", cx, 90, {70,70,95,255}, m_fontTiny);
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
        renderText(renderer, "A: toggle   Y: uninstall   X: refresh", cx, H-22, grey, m_fontTiny);
    }

    // Conflict warning overlay
    if (m_showConflict) {
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 180);
        SDL_Rect overlay = {0, 0, W, H};
        SDL_RenderFillRect(renderer, &overlay);

        SDL_SetRenderDrawColor(renderer, 30, 25, 15, 255);
        SDL_Rect card = {W/2-250, H/2-90, 500, 180};
        SDL_RenderFillRect(renderer, &card);
        SDL_SetRenderDrawColor(renderer, 200, 140, 30, 255);
        SDL_RenderDrawRect(renderer, &card);

        if (m_fontSmall) {
            renderText(renderer, "Conflict Warning", W/2-210, H/2-78, {255,200,50,255}, m_fontSmall);
            std::string msg = "This mod conflicts with: ";
            for (size_t i = 0; i < m_conflictResult.conflictingMods.size(); i++) {
                if (i > 0) msg += ", ";
                msg += m_conflictResult.conflictingMods[i];
            }
            renderText(renderer, msg, W/2-210, H/2-50, {220,200,150,255}, m_fontSmall);
            if (!m_conflictResult.conflictingFiles.empty()) {
                renderText(renderer, "Conflicting files:", W/2-210, H/2-24, {150,140,110,255}, m_fontSmall);
                int fy = H/2;
                for (auto& f : m_conflictResult.conflictingFiles) {
                    if (m_fontTiny) renderText(renderer, "  " + f, W/2-210, fy, {130,120,100,255}, m_fontTiny);
                    fy += 16;
                }
            }
        }
        if (m_fontNormal) {
            renderText(renderer, "A: Activate anyway", W/2-210, H/2+70, {200,140,30,255}, m_fontNormal);
            renderText(renderer, "B: Cancel",          W/2+60,  H/2+70, {130,130,160,255}, m_fontNormal);
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
            std::string msg = "Uninstall "" + (mod.name.empty() ? mod.id : mod.name) + ""?";
            renderText(renderer, msg,               W/2-180, H/2-50, {255,255,255,255}, m_fontSmall);
            renderText(renderer, "This deletes the mod folder permanently.", W/2-180, H/2-22, {180,130,130,255}, m_fontSmall);
        }
        if (m_fontNormal) {
            renderText(renderer, "A: Confirm",  W/2-140, H/2+16, {220,70,70,255},   m_fontNormal);
            renderText(renderer, "B: Cancel",   W/2+20,  H/2+16, {130,130,160,255}, m_fontNormal);
        }
    }
}

void MainLayout::renderSettings(SDL_Renderer* renderer) {
    const int W = m_app->screenWidth();
    const int H = m_app->screenHeight();
    SDL_SetRenderDrawColor(renderer, 15, 15, 25, 255);
    SDL_Rect bg = {SIDEBAR_W, 0, W-SIDEBAR_W, H};
    SDL_RenderFillRect(renderer, &bg);
    if (m_fontNormal) renderText(renderer, "Settings", SIDEBAR_W+30, 30, {255,255,255,255}, m_fontNormal);
    if (m_fontSmall) {
        std::string repo = m_config.repos.empty() ? "None" : m_config.repos[0];
        renderText(renderer, "Repository:", SIDEBAR_W+30, 80,  {110,110,140,255}, m_fontSmall);
        renderText(renderer, repo,          SIDEBAR_W+30, 104, {200,200,220,255}, m_fontSmall);
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
    SDL_Surface* s = TTF_RenderText_Blended(font, text.c_str(), color);
    if (!s) return;
    SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
    if (t) {
        SDL_Rect dst = {x, y, s->w, s->h};
        SDL_RenderCopy(renderer, t, nullptr, &dst);
        SDL_DestroyTexture(t);
    }
    SDL_FreeSurface(s);
}
