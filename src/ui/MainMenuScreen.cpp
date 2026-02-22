#include "MainMenuScreen.h"
#include "app/App.h"
#include "ui/BrowseScreen.h"
#include "util/Logger.h"

#include <SDL2/SDL_ttf.h>

static constexpr const char* FONT_PATH = "/vol/content/fonts/Roboto-Regular.ttf";

MainMenuScreen::MainMenuScreen(App* app) : Screen(app) {
    m_items = {
        { "Browse Mods" },
        { "Installed"   },
        { "Settings"    },
        { "Exit"        },
    };
}

MainMenuScreen::~MainMenuScreen() {
    if (m_fontLarge) TTF_CloseFont(m_fontLarge);
    if (m_fontSmall) TTF_CloseFont(m_fontSmall);
}

void MainMenuScreen::onEnter() {
    m_fontLarge = TTF_OpenFont(FONT_PATH, 48);
    m_fontSmall = TTF_OpenFont(FONT_PATH, 24);
}

void MainMenuScreen::onExit() {}

void MainMenuScreen::handleInput(const Input& input) {
    if (input.up)
        m_selectedIndex = (m_selectedIndex - 1 + (int)m_items.size()) % (int)m_items.size();
    if (input.down)
        m_selectedIndex = (m_selectedIndex + 1) % (int)m_items.size();
    if (input.a)
        handleSelect();
    if (input.b)
        m_app->quit();
}

void MainMenuScreen::handleSelect() {
    switch (m_selectedIndex) {
        case 0: m_app->pushScreen(std::make_unique<BrowseScreen>(m_app)); break;
        case 3: m_app->quit(); break;
        default: break;
    }
}

void MainMenuScreen::update() {}

void MainMenuScreen::render(SDL_Renderer* renderer) {
    const int W = m_app->screenWidth();
    const int H = m_app->screenHeight();

    SDL_Color white  = {255, 255, 255, 255};
    SDL_Color accent = { 80, 180, 255, 255};
    SDL_Color grey   = {150, 150, 150, 255};

    if (!m_fontLarge || !m_fontSmall) {
        SDL_SetRenderDrawColor(renderer, 255, 80, 0, 255);
        SDL_Rect r = {W/2 - 200, 80, 400, 60};
        SDL_RenderFillRect(renderer, &r);
        return;
    }

    renderText(renderer, "Wii U Mod Store", W/2 - 220, 80, white, m_fontLarge);

    int startY  = 250;
    int spacing = 60;
    for (int i = 0; i < (int)m_items.size(); i++) {
        SDL_Color color = (i == m_selectedIndex) ? accent : grey;
        if (i == m_selectedIndex) {
            SDL_SetRenderDrawColor(renderer, 30, 30, 50, 255);
            SDL_Rect bg = {W/2 - 160, startY + i*spacing - 8, 320, 44};
            SDL_RenderFillRect(renderer, &bg);
            SDL_SetRenderDrawColor(renderer, accent.r, accent.g, accent.b, 255);
            SDL_RenderDrawRect(renderer, &bg);
        }
        renderText(renderer, m_items[i].label, W/2 - 100, startY + i*spacing, color, m_fontSmall);
    }

    renderText(renderer, "D-Pad: Navigate   A: Select   B: Quit",
               W/2 - 180, H - 50, grey, m_fontSmall);
}

void MainMenuScreen::renderText(SDL_Renderer* renderer, const std::string& text,
                                int x, int y, SDL_Color color, TTF_Font* font) {
    if (!font) return;
    SDL_Surface* surface = TTF_RenderUTF8_Blended(font, text.c_str(), color);
    if (!surface) return;
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (texture) {
        SDL_Rect dst = {x, y, surface->w, surface->h};
        SDL_RenderCopy(renderer, texture, nullptr, &dst);
        SDL_DestroyTexture(texture);
    }
    SDL_FreeSurface(surface);
}
