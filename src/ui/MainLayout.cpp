#include "MainLayout.h"
#include "app/App.h"
#include "ui/DetailScreen.h"
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

    if (!m_showOnboarding) {
        m_fetchState = FetchState::Loading;
        m_fetchThread = std::thread([this]() {
            if (m_repo.fetch(m_config.repos[0]))
                m_fetchState = FetchState::Done;
            else {
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
        if (m_activeTab == Tab::Browse)         m_activeTab = Tab::Installed;
        else if (m_activeTab == Tab::Installed) m_activeTab = Tab::Settings;
    }

    if (m_activeTab == Tab::Browse && m_fetchState == FetchState::Done) {
        const auto& games = m_repo.repo().games;
        if (games.empty()) return;

        int modCount = (int)games[m_selectedGame].mods.size();
        int cols = CARDS_PER_ROW;

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

        // Game filter: L2/R2 or ZL/ZR — for now reuse as placeholder
        // TODO: add ZL/ZR for game switching once we have more games

        const auto& mods2 = games[m_selectedGame].mods;
        if (input.a && !mods2.empty()) {
            const auto& game = games[m_selectedGame];
            m_app->pushScreen(std::make_unique<DetailScreen>(
                m_app, game.mods[m_selectedMod], game.name));
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

    SDL_Color white  = {255,255,255,255};
    SDL_Color grey   = {160,160,180,255};
    SDL_Color accent = { 80,180,255,255};

    if (m_fontNormal)
        renderText(renderer, "Welcome to Wii U Mod Store", W/2-250, H/2-135, white, m_fontNormal);
    if (m_fontSmall) {
        renderText(renderer, "No repository configured yet.",       W/2-200, H/2- 80, grey,   m_fontSmall);
        renderText(renderer, "Add a repo URL to:",                  W/2-200, H/2- 50, grey,   m_fontSmall);
        renderText(renderer, "sd:/wiiu/apps/modstore/config.json",  W/2-200, H/2- 20, accent, m_fontSmall);
        renderText(renderer, "Example:",                            W/2-200, H/2+ 20, grey,   m_fontSmall);
        renderText(renderer, "{ \"repos\": [\"https://...\"] }",   W/2-200, H/2+ 45, grey,   m_fontSmall);
        renderText(renderer, "Press A to retry",                    W/2- 80, H/2+110, accent, m_fontSmall);
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
    SDL_RenderDrawLine(renderer, 15, 75, SIDEBAR_W-15, 75);

    struct TabEntry { const char* label; Tab tab; };
    TabEntry tabs[] = {
        {"Browse",    Tab::Browse   },
        {"Installed", Tab::Installed},
        {"Settings",  Tab::Settings },
    };

    int tabY = 100;
    for (auto& t : tabs) {
        bool active = (m_activeTab == t.tab);
        if (active) {
            SDL_SetRenderDrawColor(renderer, 40, 40, 65, 255);
            SDL_Rect bg = {5, tabY-6, SIDEBAR_W-10, 40};
            SDL_RenderFillRect(renderer, &bg);
            SDL_SetRenderDrawColor(renderer, 80, 180, 255, 255);
            SDL_Rect accent = {5, tabY-6, 4, 40};
            SDL_RenderFillRect(renderer, &accent);
        }
        SDL_Color color = active ? SDL_Color{80,180,255,255} : SDL_Color{160,160,180,255};
        if (m_fontNormal)
            renderText(renderer, t.label, 22, tabY, color, m_fontNormal);
        tabY += 60;
    }

    if (m_fontSmall)
        renderText(renderer, "L/R: switch tab", 12, H-35, {100,100,120,255}, m_fontSmall);
}

void MainLayout::renderContent(SDL_Renderer* renderer) {
    const int W  = m_app->screenWidth();
    const int H  = m_app->screenHeight();
    const int X  = SIDEBAR_W + 1;
    const int CW = W - X;

    SDL_SetRenderDrawColor(renderer, 15, 15, 25, 255);
    SDL_Rect bg = {X, 0, CW, H};
    SDL_RenderFillRect(renderer, &bg);

    switch (m_activeTab) {
        case Tab::Browse:
            renderBrowse(renderer, X, CW, H);
            break;
        case Tab::Installed:
            if (m_fontNormal) renderText(renderer, "Installed",          X+40, 60,  {255,255,255,255}, m_fontNormal);
            if (m_fontSmall)  renderText(renderer, "No mods installed.", X+40, 100, {120,120,140,255}, m_fontSmall);
            break;
        case Tab::Settings:
            if (m_fontNormal) renderText(renderer, "Settings", X+40, 60, {255,255,255,255}, m_fontNormal);
            if (m_fontSmall) {
                renderText(renderer, "Repository:", X+40, 110, {120,120,140,255}, m_fontSmall);
                std::string url = m_config.repos.empty() ? "None" : m_config.repos[0];
                renderText(renderer, url, X+40, 135, {80,180,255,255}, m_fontSmall);
            }
            break;
    }
}

void MainLayout::renderBrowse(SDL_Renderer* renderer, int x, int w, int h) {
    auto state = m_fetchState.load();

    if (state == FetchState::Loading) {
        if (m_fontNormal)
            renderText(renderer, "Loading repository...", x+40, h/2-14, {120,120,140,255}, m_fontNormal);
        return;
    }
    if (state == FetchState::Error) {
        if (m_fontNormal) renderText(renderer, "Failed to load repository.", x+40, h/2-30, {220,80,80,255}, m_fontNormal);
        if (m_fontSmall)  renderText(renderer, m_fetchError,                 x+40, h/2+10, {120,120,140,255}, m_fontSmall);
        return;
    }
    if (state != FetchState::Done) return;

    const auto& games = m_repo.repo().games;
    if (games.empty()) {
        if (m_fontSmall) renderText(renderer, "No games found.", x+40, h/2, {120,120,140,255}, m_fontSmall);
        return;
    }

    // --- Game filter bar ---
    int gx = x + 16;
    int gy = 12;
    for (int i = 0; i < (int)games.size(); i++) {
        bool sel = (i == m_selectedGame);
        SDL_Color bg   = sel ? SDL_Color{50,100,200,255} : SDL_Color{30,30,48,255};
        SDL_Color text = sel ? SDL_Color{255,255,255,255} : SDL_Color{160,160,180,255};

        int tw = 120;
        SDL_Rect pill = {gx, gy, tw, 30};
        SDL_SetRenderDrawColor(renderer, bg.r, bg.g, bg.b, 255);
        SDL_RenderFillRect(renderer, &pill);
        if (sel) {
            SDL_SetRenderDrawColor(renderer, 80, 180, 255, 255);
            SDL_RenderDrawRect(renderer, &pill);
        }
        if (m_fontTiny)
            renderText(renderer, games[i].name, gx+8, gy+7, text, m_fontTiny);
        gx += tw + 10;
    }

    // Divider
    SDL_SetRenderDrawColor(renderer, 40, 40, 60, 255);
    SDL_RenderDrawLine(renderer, x+10, 52, x+w-10, 52);

    // --- Cards grid ---
    const auto& mods = games[m_selectedGame].mods;
    if (mods.empty()) {
        if (m_fontSmall) renderText(renderer, "No mods for this game.", x+40, 90, {120,120,140,255}, m_fontSmall);
        return;
    }

    int gridX = x + CARD_PAD;
    int gridY = GRID_TOP;

    for (int i = 0; i < (int)mods.size(); i++) {
        int col = i % CARDS_PER_ROW;
        int row = i / CARDS_PER_ROW;

        int cx = gridX + col * (CARD_W + CARD_PAD);
        int cy = gridY + row * (CARD_H + CARD_PAD);

        bool sel = (i == m_selectedMod);
        const auto& mod = mods[i];

        // Card background
        SDL_SetRenderDrawColor(renderer, 25, 25, 40, 255);
        SDL_Rect card = {cx, cy, CARD_W, CARD_H};
        SDL_RenderFillRect(renderer, &card);

        // Border: accent if selected, subtle if not
        if (sel) {
            SDL_SetRenderDrawColor(renderer, 80, 180, 255, 255);
        } else {
            SDL_SetRenderDrawColor(renderer, 45, 45, 65, 255);
        }
        SDL_RenderDrawRect(renderer, &card);

        // Thumbnail placeholder area
        SDL_SetRenderDrawColor(renderer, 35, 35, 55, 255);
        SDL_Rect thumb = {cx+1, cy+1, CARD_W-2, 90};
        SDL_RenderFillRect(renderer, &thumb);

        // Placeholder icon lines
        SDL_SetRenderDrawColor(renderer, 55, 55, 80, 255);
        SDL_RenderDrawLine(renderer, cx+CARD_W/2-20, cy+45, cx+CARD_W/2+20, cy+45);
        SDL_RenderDrawLine(renderer, cx+CARD_W/2,    cy+25, cx+CARD_W/2,    cy+65);

        // Type badge
        bool isModpack = (mod.type == "modpack");
        SDL_Color badgeBg   = isModpack ? SDL_Color{120,60,180,255} : SDL_Color{40,100,180,255};
        SDL_Color badgeText = {255,255,255,255};
        SDL_Rect badge = {cx + CARD_W - 78, cy + 6, 70, 18};
        SDL_SetRenderDrawColor(renderer, badgeBg.r, badgeBg.g, badgeBg.b, 255);
        SDL_RenderFillRect(renderer, &badge);
        if (m_fontTiny)
            renderText(renderer, isModpack ? "MODPACK" : "MOD", badge.x+6, badge.y+2, badgeText, m_fontTiny);

        // Mod name
        SDL_Color nameColor = sel ? SDL_Color{255,255,255,255} : SDL_Color{210,210,230,255};
        if (m_fontSmall)
            renderText(renderer, mod.name, cx+8, cy+96, nameColor, m_fontSmall);

        // Author + version
        std::string meta = mod.author + "  v" + mod.version;
        if (m_fontTiny)
            renderText(renderer, meta, cx+8, cy+120, {120,120,145,255}, m_fontTiny);
    }

    // Bottom hint
    if (m_fontTiny)
        renderText(renderer, "D-Pad: navigate   A: details   L/R: switch tab",
                   x+20, h-25, {70,70,95,255}, m_fontTiny);
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
