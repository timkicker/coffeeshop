#include "BrowseScreen.h"
#include "app/App.h"
#include "util/TextCache.h"

static constexpr const char* FONT_PATH = "/vol/content/fonts/Roboto-Regular.ttf";

BrowseScreen::BrowseScreen(App* app) : Screen(app) {}

BrowseScreen::~BrowseScreen() {
    if (m_font) TTF_CloseFont(m_font);
}

void BrowseScreen::onEnter() {
    m_font = TTF_OpenFont(FONT_PATH, 32);
}

void BrowseScreen::handleInput(const Input& input) {
    if (input.b) m_app->popScreen();
}

void BrowseScreen::update() {}

void BrowseScreen::render(SDL_Renderer* renderer) {
    const int W = m_app->screenWidth();
    const int H = m_app->screenHeight();
    SDL_Color white = {255, 255, 255, 255};
    SDL_Color grey  = {150, 150, 150, 255};

    if (m_font) {
        renderText(renderer, "Browse Mods",   W/2 - 120, 80,  white, m_font);
        renderText(renderer, "(coming soon)", W/2 - 100, 200, grey,  m_font);
        renderText(renderer, "B: Back",       40, H - 50,     grey,  m_font);
    }
}

void BrowseScreen::renderText(SDL_Renderer* renderer, const std::string& text,
                               int x, int y, SDL_Color color, TTF_Font* font) {
    if (!font || text.empty()) return;
    SDL_Texture* t = TextCache::get().texture(renderer, font, color, text);
    if (!t) return;
    int w = 0, h = 0;
    TextCache::get().sizeOf(t, &w, &h);
    SDL_Rect dst = {x, y, w, h};
    SDL_RenderCopy(renderer, t, nullptr, &dst);
}
