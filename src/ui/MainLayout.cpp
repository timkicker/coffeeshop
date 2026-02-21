#include "MainLayout.h"
#include "app/App.h"
#include "util/Logger.h"

static constexpr const char* FONT_PATH = "/vol/content/Roboto-Regular.ttf";

MainLayout::MainLayout(App* app) : Screen(app) {}

MainLayout::~MainLayout() {
    if (m_fetchThread.joinable()) m_fetchThread.join();
    if (m_fontNormal) TTF_CloseFont(m_fontNormal);
    if (m_fontSmall)  TTF_CloseFont(m_fontSmall);
}

void MainLayout::onEnter() {
    m_fontNormal = TTF_OpenFont(FONT_PATH, 28);
    m_fontSmall  = TTF_OpenFont(FONT_PATH, 20);

    m_config.load();
    m_showOnboarding = !m_config.hasRepos();

    if (!m_showOnboarding) {
        m_fetchState = FetchState::Loading;
        m_fetchThread = std::thread([this]() {
            if (m_repo.fetch(m_config.repos[0])) {
                m_fetchState = FetchState::Done;
            } else {
                m_fetchError = m_repo.lastError();
                m_fetchState = FetchState::Error;
            }
        });
    }
}

void MainLayout::onExit() {
    if (m_fetchThread.joinable()) m_fetchThread.join();
}

void MainLayout::handleInput(const Input& input) {
    if (m_showOnboarding) {
        if (input.a) {
            m_config.load();
            m_showOnboarding = !m_config.hasRepos();
        }
        return;
    }

    if (input.l) {
        if (m_activeTab == Tab::Installed) m_activeTab = Tab::Browse;
        else if (m_activeTab == Tab::Settings) m_activeTab = Tab::Installed;
    }
    if (input.r) {
        if (m_activeTab == Tab::Browse)    m_activeTab = Tab::Installed;
        else if (m_activeTab == Tab::Installed) m_activeTab = Tab::Settings;
    }

    // Browse navigation (only when loaded)
    if (m_activeTab == Tab::Browse && m_fetchState == FetchState::Done) {
        const auto& games = m_repo.repo().games;
        if (games.empty()) return;

        if (input.up)   m_selectedMod = std::max(0, m_selectedMod - 1);
        if (input.down) {
            int maxMod = (int)games[m_selectedGame].mods.size() - 1;
            m_selectedMod = std::min(maxMod, m_selectedMod + 1);
        }
        if (input.left) {
            m_selectedGame = std::max(0, m_selectedGame - 1);
            m_selectedMod  = 0;
        }
        if (input.right) {
            m_selectedGame = std::min((int)games.size() - 1, m_selectedGame + 1);
            m_selectedMod  = 0;
        }
    }
}

void MainLayout::update() {}

void MainLayout::render(SDL_Renderer* renderer) {
    if (m_showOnboarding) { renderOnboarding(renderer); return; }
    renderSidebar(renderer);
    renderContent(renderer);
}

void MainLayout::renderOnboarding(SDL_Renderer* renderer) {
    const int W = m_app->screenWidth();
    const int H = m_app->screenHeight();

    SDL_SetRenderDrawColor(renderer, 15, 15, 25, 255);
    SDL_Rect bg = {0, 0, W, H};
    SDL_RenderFillRect(renderer, &bg);

    SDL_SetRenderDrawColor(renderer, 28, 28, 45, 255);
    SDL_Rect card = {W/2 - 320, H/2 - 160, 640, 320};
    SDL_RenderFillRect(renderer, &card);
    SDL_SetRenderDrawColor(renderer, 80, 180, 255, 255);
    SDL_RenderDrawRect(renderer, &card);

    SDL_Color white  = {255, 255, 255, 255};
    SDL_Color grey   = {160, 160, 180, 255};
    SDL_Color accent = { 80, 180, 255, 255};

    if (m_fontNormal)
        renderText(renderer, "Welcome to Wii U Mod Store", W/2 - 250, H/2 - 135, white, m_fontNormal);
    if (m_fontSmall) {
        renderText(renderer, "No repository configured yet.",        W/2 - 200, H/2 -  80, grey,   m_fontSmall);
        renderText(renderer, "Add a repo URL to:",                   W/2 - 200, H/2 -  50, grey,   m_fontSmall);
        renderText(renderer, "sd:/wiiu/apps/modstore/config.json",   W/2 - 200, H/2 -  20, accent, m_fontSmall);
        renderText(renderer, "Example:",                             W/2 - 200, H/2 +  20, grey,   m_fontSmall);
        renderText(renderer, "{ \"repos\": [\"https://...\"] }",    W/2 - 200, H/2 +  45, grey,   m_fontSmall);
        renderText(renderer, "Press A to retry",                     W/2 -  80, H/2 + 110, accent, m_fontSmall);
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
        renderText(renderer, "Mod Store", 20, 30, {255,255,255,255}, m_fontNormal);

    SDL_SetRenderDrawColor(renderer, 50, 50, 70, 255);
    SDL_RenderDrawLine(renderer, 15, 75, SIDEBAR_W - 15, 75);

    struct TabEntry { const char* label; Tab tab; };
    TabEntry tabs[] = {
        { "Browse",    Tab::Browse    },
        { "Installed", Tab::Installed },
        { "Settings",  Tab::Settings  },
    };

    int tabY = 100;
    for (auto& t : tabs) {
        bool active = (m_activeTab == t.tab);
        if (active) {
            SDL_SetRenderDrawColor(renderer, 40, 40, 65, 255);
            SDL_Rect bg = {5, tabY - 6, SIDEBAR_W - 10, 40};
            SDL_RenderFillRect(renderer, &bg);
            SDL_SetRenderDrawColor(renderer, 80, 180, 255, 255);
            SDL_Rect accent = {5, tabY - 6, 4, 40};
            SDL_RenderFillRect(renderer, &accent);
        }
        SDL_Color color = active ? SDL_Color{80,180,255,255} : SDL_Color{160,160,180,255};
        if (m_fontNormal)
            renderText(renderer, t.label, 22, tabY, color, m_fontNormal);
        tabY += 60;
    }

    if (m_fontSmall)
        renderText(renderer, "L/R: switch tab", 12, H - 35, {100,100,120,255}, m_fontSmall);
}

void MainLayout::renderContent(SDL_Renderer* renderer) {
    const int W = m_app->screenWidth();
    const int H = m_app->screenHeight();
    const int X = SIDEBAR_W + 1;
    const int CW = W - X;

    SDL_SetRenderDrawColor(renderer, 15, 15, 25, 255);
    SDL_Rect bg = {X, 0, CW, H};
    SDL_RenderFillRect(renderer, &bg);

    SDL_Color white = {255,255,255,255};
    SDL_Color grey  = {120,120,140,255};

    switch (m_activeTab) {
        case Tab::Browse:
            renderBrowse(renderer, X, CW, H);
            break;
        case Tab::Installed:
            if (m_fontNormal) renderText(renderer, "Installed",          X+40, 60,  white, m_fontNormal);
            if (m_fontSmall)  renderText(renderer, "No mods installed.", X+40, 110, grey,  m_fontSmall);
            break;
        case Tab::Settings:
            if (m_fontNormal) renderText(renderer, "Settings", X+40, 60, white, m_fontNormal);
            if (m_fontSmall) {
                std::string repoStr = m_config.repos.empty()
                    ? "No repository configured."
                    : m_config.repos[0];
                renderText(renderer, "Repository:", X+40, 110, grey, m_fontSmall);
                renderText(renderer, repoStr,       X+40, 140, {80,180,255,255}, m_fontSmall);
            }
            break;
    }
}

void MainLayout::renderBrowse(SDL_Renderer* renderer, int x, int w, int h) {
    SDL_Color white  = {255,255,255,255};
    SDL_Color grey   = {120,120,140,255};
    SDL_Color accent = { 80,180,255,255};

    auto state = m_fetchState.load();

    if (state == FetchState::Loading) {
        if (m_fontNormal) renderText(renderer, "Loading repository...", x+40, h/2 - 20, grey, m_fontNormal);
        return;
    }
    if (state == FetchState::Error) {
        if (m_fontNormal) renderText(renderer, "Failed to load repository.", x+40, h/2 - 30, {220,80,80,255}, m_fontNormal);
        if (m_fontSmall)  renderText(renderer, m_fetchError, x+40, h/2 + 10, grey, m_fontSmall);
        return;
    }
    if (state != FetchState::Done) return;

    const auto& repo  = m_repo.repo();
    const auto& games = repo.games;

    if (games.empty()) {
        if (m_fontNormal) renderText(renderer, "No games found in repository.", x+40, h/2, grey, m_fontNormal);
        return;
    }

    // Game filter bar
    int gx = x + 20;
    for (int i = 0; i < (int)games.size(); i++) {
        bool sel = (i == m_selectedGame);
        SDL_Color col = sel ? accent : grey;
        if (sel) {
            SDL_SetRenderDrawColor(renderer, 40, 40, 65, 255);
            SDL_Rect bg = {gx - 8, 10, 140, 36};
            SDL_RenderFillRect(renderer, &bg);
        }
        if (m_fontSmall) renderText(renderer, games[i].name, gx, 18, col, m_fontSmall);
        gx += 160;
    }

    // Divider
    SDL_SetRenderDrawColor(renderer, 50, 50, 70, 255);
    SDL_RenderDrawLine(renderer, x+10, 55, x+w-10, 55);

    // Mod list
    const auto& mods = games[m_selectedGame].mods;
    if (mods.empty()) {
        if (m_fontSmall) renderText(renderer, "No mods for this game.", x+40, 90, grey, m_fontSmall);
        return;
    }

    int my = 70;
    for (int i = 0; i < (int)mods.size(); i++) {
        bool sel = (i == m_selectedMod);
        const auto& mod = mods[i];

        if (sel) {
            SDL_SetRenderDrawColor(renderer, 30, 30, 50, 255);
            SDL_Rect bg = {x+10, my - 4, w - 20, 52};
            SDL_RenderFillRect(renderer, &bg);
            SDL_SetRenderDrawColor(renderer, 80, 180, 255, 255);
            SDL_Rect bar = {x+10, my - 4, 4, 52};
            SDL_RenderFillRect(renderer, &bar);
        }

        SDL_Color nameCol = sel ? white : SDL_Color{200,200,220,255};
        if (m_fontNormal) renderText(renderer, mod.name,   x+25, my,      nameCol, m_fontNormal);

        std::string meta = mod.author + "  v" + mod.version + "  [" + mod.type + "]";
        if (m_fontSmall)  renderText(renderer, meta,       x+25, my + 28, grey,    m_fontSmall);

        my += 65;
        if (my > h - 40) break;
    }

    if (m_fontSmall)
        renderText(renderer, "Up/Down: select   Left/Right: switch game   A: details",
                   x+20, h - 35, {80,80,100,255}, m_fontSmall);
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
