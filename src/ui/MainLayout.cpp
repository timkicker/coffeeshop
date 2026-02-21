#include "MainLayout.h"
#include "app/App.h"
#include "app/Config.h"
#include "util/Logger.h"

#include <SDL2/SDL_ttf.h>

static constexpr const char* FONT_PATH = "/vol/content/Roboto-Regular.ttf";

MainLayout::MainLayout(App* app) : Screen(app) {}

MainLayout::~MainLayout() {
    if (m_fontNormal) TTF_CloseFont(m_fontNormal);
    if (m_fontSmall)  TTF_CloseFont(m_fontSmall);
}

void MainLayout::onEnter() {
    m_fontNormal = TTF_OpenFont(FONT_PATH, 28);
    m_fontSmall  = TTF_OpenFont(FONT_PATH, 20);

    m_config.load();
    m_showOnboarding = !m_config.hasRepos();
}

void MainLayout::onExit() {}

void MainLayout::handleInput(const Input& input) {
    if (m_showOnboarding) {
        // Any button dismisses onboarding (user should have edited config by now)
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
}

void MainLayout::update() {}

void MainLayout::render(SDL_Renderer* renderer) {
    if (m_showOnboarding) {
        renderOnboarding(renderer);
        return;
    }
    renderSidebar(renderer);
    renderContent(renderer);
}

void MainLayout::renderOnboarding(SDL_Renderer* renderer) {
    const int W = m_app->screenWidth();
    const int H = m_app->screenHeight();

    // Dimmed background
    SDL_SetRenderDrawColor(renderer, 15, 15, 25, 255);
    SDL_Rect bg = {0, 0, W, H};
    SDL_RenderFillRect(renderer, &bg);

    // Card
    SDL_SetRenderDrawColor(renderer, 28, 28, 45, 255);
    SDL_Rect card = {W/2 - 320, H/2 - 160, 640, 320};
    SDL_RenderFillRect(renderer, &card);
    SDL_SetRenderDrawColor(renderer, 80, 180, 255, 255);
    SDL_RenderDrawRect(renderer, &card);

    SDL_Color white  = {255, 255, 255, 255};
    SDL_Color grey   = {160, 160, 180, 255};
    SDL_Color accent = { 80, 180, 255, 255};

    if (m_fontNormal) {
        renderText(renderer, "Welcome to Wii U Mod Store",
                   W/2 - 250, H/2 - 135, white, m_fontNormal);
    }
    if (m_fontSmall) {
        renderText(renderer, "No repository configured yet.",
                   W/2 - 200, H/2 - 80, grey, m_fontSmall);
        renderText(renderer, "Add a repo URL to:",
                   W/2 - 200, H/2 - 50, grey, m_fontSmall);
        renderText(renderer, "sd:/wiiu/apps/modstore/config.json",
                   W/2 - 200, H/2 - 20, accent, m_fontSmall);
        renderText(renderer, "Example:",
                   W/2 - 200, H/2 + 20, grey, m_fontSmall);
        renderText(renderer, "{ \"repos\": [\"https://your-repo/repo.json\"] }",
                   W/2 - 200, H/2 + 45, grey, m_fontSmall);
        renderText(renderer, "Press A to retry",
                   W/2 - 80, H/2 + 110, accent, m_fontSmall);
    }
}

void MainLayout::renderSidebar(SDL_Renderer* renderer) {
    const int H = m_app->screenHeight();

    SDL_SetRenderDrawColor(renderer, 22, 22, 35, 255);
    SDL_Rect sidebar = {0, 0, SIDEBAR_W, H};
    SDL_RenderFillRect(renderer, &sidebar);

    SDL_SetRenderDrawColor(renderer, 50, 50, 70, 255);
    SDL_RenderDrawLine(renderer, SIDEBAR_W, 0, SIDEBAR_W, H);

    SDL_Color titleColor = {255, 255, 255, 255};
    if (m_fontNormal)
        renderText(renderer, "Mod Store", 20, 30, titleColor, m_fontNormal);

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
        SDL_Color color = active
            ? SDL_Color{80, 180, 255, 255}
            : SDL_Color{160, 160, 180, 255};
        if (m_fontNormal)
            renderText(renderer, t.label, 22, tabY, color, m_fontNormal);
        tabY += 60;
    }

    SDL_Color grey = {100, 100, 120, 255};
    if (m_fontSmall)
        renderText(renderer, "L/R: switch tab", 12, H - 35, grey, m_fontSmall);
}

void MainLayout::renderContent(SDL_Renderer* renderer) {
    const int W = m_app->screenWidth();
    const int H = m_app->screenHeight();
    const int contentX = SIDEBAR_W + 1;
    const int contentW = W - contentX;

    SDL_SetRenderDrawColor(renderer, 15, 15, 25, 255);
    SDL_Rect bg = {contentX, 0, contentW, H};
    SDL_RenderFillRect(renderer, &bg);

    SDL_Color white = {255, 255, 255, 255};
    SDL_Color grey  = {120, 120, 140, 255};

    if (!m_fontNormal) return;

    switch (m_activeTab) {
        case Tab::Browse:
            renderText(renderer, "Browse Mods",      contentX + 40, 60,  white, m_fontNormal);
            if (m_fontSmall)
                renderText(renderer, "Loading repository...", contentX + 40, 110, grey, m_fontSmall);
            break;
        case Tab::Installed:
            renderText(renderer, "Installed",         contentX + 40, 60,  white, m_fontNormal);
            if (m_fontSmall)
                renderText(renderer, "No mods installed.", contentX + 40, 110, grey, m_fontSmall);
            break;
        case Tab::Settings:
            renderText(renderer, "Settings",          contentX + 40, 60,  white, m_fontNormal);
            if (m_fontSmall && !m_config.repos.empty())
                renderText(renderer, m_config.repos[0], contentX + 40, 110, grey, m_fontSmall);
            else if (m_fontSmall)
                renderText(renderer, "No repository configured.", contentX + 40, 110, grey, m_fontSmall);
            break;
    }
}

void MainLayout::renderText(SDL_Renderer* renderer, const std::string& text,
                             int x, int y, SDL_Color color, TTF_Font* font) {
    if (!font) return;
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
