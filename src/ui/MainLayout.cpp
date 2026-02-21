#include "MainLayout.h"
#include "app/App.h"
#include "ui/DetailScreen.h"
#include "ui/DownloadQueueScreen.h"
#include "net/DownloadQueue.h"
#include "util/Logger.h"

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
            RepoManager rm;
            rm.fetch(m_config.repos[0]);
            std::lock_guard<std::mutex> lock(m_repoMutex);
            m_repo      = rm.repo();
            m_fetchState = rm.lastError().empty() ? FetchState::Done : FetchState::Error;
        });
    }
}

void MainLayout::onExit() {}

void MainLayout::handleInput(const Input& input) {
    if (m_showOnboarding) {
        if (input.a) {
            m_config.load();
            m_showOnboarding = !m_config.hasRepos();
        }
        return;
    }

    // Y button: open download queue
    if (input.y) {
        m_app->pushScreen(std::make_unique<DownloadQueueScreen>(m_app));
        return;
    }

    // L/R: switch tabs
    if (input.l) m_activeTab = (m_activeTab == Tab::Browse)    ? Tab::Browse :
                                (m_activeTab == Tab::Installed) ? Tab::Browse : Tab::Installed;
    if (input.r) m_activeTab = (m_activeTab == Tab::Browse)    ? Tab::Installed :
                                (m_activeTab == Tab::Installed) ? Tab::Settings : Tab::Settings;

    if (m_activeTab != Tab::Browse) return;

    std::lock_guard<std::mutex> lock(m_repoMutex);
    auto& games = m_repo.games;
    if (games.empty()) return;

    auto& mods     = games[m_selectedGame].mods;
    int   modCount = (int)mods.size();
    int   cols     = CARDS_PER_ROW;

    if (input.right && (m_selectedMod % cols) < cols - 1 && m_selectedMod + 1 < modCount)
        m_selectedMod++;
    if (input.left && (m_selectedMod % cols) > 0)
        m_selectedMod--;
    if (input.down) {
        int next = m_selectedMod + cols;
        if (next < modCount) m_selectedMod = next;
    }
    if (input.up) {
        int prev = m_selectedMod - cols;
        if (prev >= 0) m_selectedMod = prev;
    }

    const auto& mods2 = games[m_selectedGame].mods;
    if (input.a && !mods2.empty()) {
        const auto& game = games[m_selectedGame];
        m_app->pushScreen(std::make_unique<DetailScreen>(
            m_app, game.mods[m_selectedMod], game.name, game.titleIds));
    }
}

void MainLayout::update() {}

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
    SDL_RenderDrawLine(renderer, 15, 58, SIDEBAR_W - 15, 58);

    struct TabEntry { const char* label; Tab tab; };
    TabEntry tabs[] = {
        { "Browse",    Tab::Browse    },
        { "Installed", Tab::Installed },
        { "Settings",  Tab::Settings  },
    };

    int tabY = 75;
    for (auto& t : tabs) {
        bool active = (m_activeTab == t.tab);
        if (active) {
            SDL_SetRenderDrawColor(renderer, 40, 40, 65, 255);
            SDL_Rect bg = {5, tabY - 4, SIDEBAR_W - 10, 36};
            SDL_RenderFillRect(renderer, &bg);
            SDL_SetRenderDrawColor(renderer, 80, 180, 255, 255);
            SDL_Rect accent = {5, tabY - 4, 4, 36};
            SDL_RenderFillRect(renderer, &accent);
        }
        SDL_Color color = active ? SDL_Color{80,180,255,255} : SDL_Color{160,160,180,255};
        if (m_fontNormal)
            renderText(renderer, t.label, 22, tabY, color, m_fontNormal);
        tabY += 52;
    }

    // Download badge
    int activeDownloads = DownloadQueue::get().activeCount();
    if (activeDownloads > 0 && m_fontTiny) {
        std::string badge = std::to_string(activeDownloads);
        SDL_SetRenderDrawColor(renderer, 220, 70, 70, 255);
        SDL_Rect dot = {SIDEBAR_W - 22, 12, 18, 18};
        SDL_RenderFillRect(renderer, &dot);
        renderText(renderer, badge, SIDEBAR_W - 18, 14, {255,255,255,255}, m_fontTiny);
    }

    // Bottom hints
    SDL_Color grey = {80, 80, 105, 255};
    if (m_fontTiny) {
        renderText(renderer, "L/R: switch tab", 12, H - 44, grey, m_fontTiny);
        renderText(renderer, "Y: downloads",    12, H - 26, grey, m_fontTiny);
    }
}

void MainLayout::renderBrowse(SDL_Renderer* renderer) {
    const int W = m_app->screenWidth();
    const int H = m_app->screenHeight();
    const int cx = SIDEBAR_W + 10;

    SDL_SetRenderDrawColor(renderer, 15, 15, 25, 255);
    SDL_Rect bg = {SIDEBAR_W, 0, W - SIDEBAR_W, H};
    SDL_RenderFillRect(renderer, &bg);

    std::lock_guard<std::mutex> lock(m_repoMutex);

    if (m_fetchState == FetchState::Loading) {
        if (m_fontNormal)
            renderText(renderer, "Loading repository...", cx + 20, 60, {150,150,170,255}, m_fontNormal);
        return;
    }
    if (m_fetchState == FetchState::Error) {
        if (m_fontNormal)
            renderText(renderer, "Failed to load repository.", cx + 20, 60, {220,70,70,255}, m_fontNormal);
        return;
    }

    auto& games = m_repo.games;
    if (games.empty()) {
        if (m_fontNormal)
            renderText(renderer, "No mods found.", cx + 20, 60, {150,150,170,255}, m_fontNormal);
        return;
    }

    // Game filter bar
    int gx = cx + 10;
    for (int i = 0; i < (int)games.size(); i++) {
        bool sel = (i == m_selectedGame);
        SDL_Color bg2 = sel ? SDL_Color{50,120,220,255} : SDL_Color{30,30,50,255};
        int tw = 0;
        if (m_fontSmall) TTF_SizeText(m_fontSmall, games[i].name.c_str(), &tw, nullptr);
        SDL_Rect pill = {gx, 12, tw + 20, 28};
        SDL_SetRenderDrawColor(renderer, bg2.r, bg2.g, bg2.b, 255);
        SDL_RenderFillRect(renderer, &pill);
        if (m_fontSmall)
            renderText(renderer, games[i].name, gx + 10, 16,
                       sel ? SDL_Color{255,255,255,255} : SDL_Color{160,160,190,255}, m_fontSmall);
        gx += tw + 30;
    }

    // Cards grid
    auto& mods = games[m_selectedGame].mods;
    int cardX = cx + CARD_PAD;
    int cardY = GRID_TOP;

    for (int i = 0; i < (int)mods.size(); i++) {
        auto& mod = mods[i];
        bool  sel = (i == m_selectedMod);
        int   col = i % CARDS_PER_ROW;
        int   row = i / CARDS_PER_ROW;

        int x = cx + CARD_PAD + col * (CARD_W + CARD_PAD);
        int y = GRID_TOP + row * (CARD_H + CARD_PAD);

        // Card background
        SDL_SetRenderDrawColor(renderer, 25, 25, 40, 255);
        SDL_Rect card = {x, y, CARD_W, CARD_H};
        SDL_RenderFillRect(renderer, &card);

        // Border (blue if selected)
        if (sel) {
            SDL_SetRenderDrawColor(renderer, 80, 180, 255, 255);
        } else {
            SDL_SetRenderDrawColor(renderer, 45, 45, 65, 255);
        }
        SDL_RenderDrawRect(renderer, &card);

        // Thumbnail placeholder
        SDL_SetRenderDrawColor(renderer, 32, 32, 52, 255);
        SDL_Rect thumb = {x + 4, y + 4, CARD_W - 8, 90};
        SDL_RenderFillRect(renderer, &thumb);
        SDL_SetRenderDrawColor(renderer, 50, 50, 78, 255);
        SDL_RenderDrawLine(renderer, x + CARD_W/2 - 14, y + 49, x + CARD_W/2 + 14, y + 49);
        SDL_RenderDrawLine(renderer, x + CARD_W/2, y + 35, x + CARD_W/2, y + 63);

        // Type badge
        bool isModpack = (mod.type == "modpack");
        SDL_Color badgeBg = isModpack ? SDL_Color{120,60,180,255} : SDL_Color{40,100,180,255};
        SDL_Rect badge = {x + CARD_W - (isModpack ? 70 : 46), y + 6,
                          isModpack ? 64 : 40, 20};
        SDL_SetRenderDrawColor(renderer, badgeBg.r, badgeBg.g, badgeBg.b, 255);
        SDL_RenderFillRect(renderer, &badge);
        if (m_fontTiny)
            renderText(renderer, isModpack ? "MODPACK" : "MOD",
                       badge.x + 5, badge.y + 3, {255,255,255,255}, m_fontTiny);

        // Mod name
        if (m_fontSmall)
            renderText(renderer, mod.name, x + 8, y + 100, {220,220,240,255}, m_fontSmall);

        // Author + version
        if (m_fontTiny) {
            std::string sub = mod.author + " v" + mod.version;
            renderText(renderer, sub, x + 8, y + 124, {110,110,140,255}, m_fontTiny);
        }
    }

    // Bottom hint
    if (m_fontTiny)
        renderText(renderer, "D-Pad: navigate   A: details   L/R: switch tab   Y: downloads",
                   cx + 10, H - 22, {70,70,95,255}, m_fontTiny);
}

void MainLayout::renderInstalled(SDL_Renderer* renderer) {
    const int W = m_app->screenWidth();
    const int H = m_app->screenHeight();
    SDL_SetRenderDrawColor(renderer, 15, 15, 25, 255);
    SDL_Rect bg = {SIDEBAR_W, 0, W - SIDEBAR_W, H};
    SDL_RenderFillRect(renderer, &bg);
    if (m_fontNormal)
        renderText(renderer, "No mods installed.", SIDEBAR_W + 30, 60, {150,150,170,255}, m_fontNormal);
}

void MainLayout::renderSettings(SDL_Renderer* renderer) {
    const int W = m_app->screenWidth();
    const int H = m_app->screenHeight();
    SDL_SetRenderDrawColor(renderer, 15, 15, 25, 255);
    SDL_Rect bg = {SIDEBAR_W, 0, W - SIDEBAR_W, H};
    SDL_RenderFillRect(renderer, &bg);
    if (m_fontNormal)
        renderText(renderer, "Settings", SIDEBAR_W + 30, 30, {255,255,255,255}, m_fontNormal);
    if (m_fontSmall) {
        std::string repo = m_config.repos.empty() ? "None" : m_config.repos[0];
        renderText(renderer, "Repository:", SIDEBAR_W + 30, 80, {110,110,140,255}, m_fontSmall);
        renderText(renderer, repo,          SIDEBAR_W + 30, 104, {200,200,220,255}, m_fontSmall);
    }
}

void MainLayout::renderOnboarding(SDL_Renderer* renderer) {
    const int W = m_app->screenWidth();
    const int H = m_app->screenHeight();
    SDL_SetRenderDrawColor(renderer, 15, 15, 25, 255);
    SDL_Rect bg = {0, 0, W, H};
    SDL_RenderFillRect(renderer, &bg);

    SDL_SetRenderDrawColor(renderer, 25, 25, 42, 255);
    SDL_Rect card = {W/2 - 260, H/2 - 120, 520, 240};
    SDL_RenderFillRect(renderer, &card);
    SDL_SetRenderDrawColor(renderer, 50, 50, 75, 255);
    SDL_RenderDrawRect(renderer, &card);

    if (m_fontNormal)
        renderText(renderer, "No repository configured.", W/2 - 180, H/2 - 90, {255,255,255,255}, m_fontNormal);
    if (m_fontSmall) {
        renderText(renderer, "Edit config.json and add a repo URL:", W/2 - 200, H/2 - 48, {150,150,175,255}, m_fontSmall);
        renderText(renderer, "{ \"repos\": [\"https://...\"] }",        W/2 - 160, H/2 - 20, {100,180,255,255}, m_fontSmall);
        renderText(renderer, "A: Retry",                               W/2 -  40, H/2 + 60, {100,100,130,255}, m_fontSmall);
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
