#include "BrowseScreen.h"
#include "app/App.h"

BrowseScreen::BrowseScreen(App* app) : Screen(app) {}

BrowseScreen::~BrowseScreen() {
    if (m_font) TTF_CloseFont(m_font);
}

void BrowseScreen::onEnter() {
    m_font = TTF_OpenFont("/vol/content/Roboto-Regular.ttf", 32);
}

void BrowseScreen::handleEvent(const SDL_Event& event) {
    // B button or Escape = back
    if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)
        m_app->popScreen();
    if (event.type == SDL_CONTROLLERBUTTONDOWN &&
        event.cbutton.button == SDL_CONTROLLER_BUTTON_B)
        m_app->popScreen();
}

void BrowseScreen::update() {}

void BrowseScreen::render(SDL_Renderer* renderer) {
    const int W = m_app->screenWidth();
    SDL_Color white = {255, 255, 255, 255};
    SDL_Color grey  = {150, 150, 150, 255};

    if (m_font) {
        renderText(renderer, "Browse Mods", W/2 - 120, 80, white, m_font);
        renderText(renderer, "(coming soon)", W/2 - 90, 200, grey, m_font);
        renderText(renderer, "B: Back", 40, m_app->screenHeight() - 50, grey, m_font);
    }
}

void BrowseScreen::renderText(SDL_Renderer* renderer, const std::string& text,
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
