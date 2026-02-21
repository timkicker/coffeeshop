#include "RegionSelectScreen.h"
#include "app/App.h"
#include "net/DownloadQueue.h"

static constexpr const char* FONT_PATH = "/vol/content/Roboto-Regular.ttf";

RegionSelectScreen::RegionSelectScreen(App* app, const Mod& mod,
                                       const std::vector<TitleEntry>& entries)
    : Screen(app), m_mod(mod), m_entries(entries) {}

RegionSelectScreen::~RegionSelectScreen() {
    if (m_fontLarge)  TTF_CloseFont(m_fontLarge);
    if (m_fontNormal) TTF_CloseFont(m_fontNormal);
    if (m_fontSmall)  TTF_CloseFont(m_fontSmall);
}

void RegionSelectScreen::onEnter() {
    m_fontLarge  = TTF_OpenFont(FONT_PATH, 32);
    m_fontNormal = TTF_OpenFont(FONT_PATH, 24);
    m_fontSmall  = TTF_OpenFont(FONT_PATH, 17);
}

void RegionSelectScreen::onExit() {}

void RegionSelectScreen::handleInput(const Input& input) {
    if (input.up)   m_selected = std::max(0, m_selected - 1);
    if (input.down) m_selected = std::min((int)m_entries.size() - 1, m_selected + 1);

    if (input.a) {
        DownloadQueue::get().enqueue(m_mod, m_entries[m_selected].id);
        m_app->popScreen(); // back to detail
        m_app->popScreen(); // back to browse
    }
    if (input.b) m_app->popScreen();
}

void RegionSelectScreen::update() {}

void RegionSelectScreen::render(SDL_Renderer* renderer) {
    const int W = m_app->screenWidth();
    const int H = m_app->screenHeight();

    SDL_Color white  = {255, 255, 255, 255};
    SDL_Color grey   = {130, 130, 155, 255};
    SDL_Color accent = { 80, 180, 255, 255};

    SDL_SetRenderDrawColor(renderer, 12, 12, 20, 255);
    SDL_Rect bg = {0, 0, W, H};
    SDL_RenderFillRect(renderer, &bg);

    SDL_SetRenderDrawColor(renderer, 22, 22, 35, 255);
    SDL_Rect card = {W/2 - 280, H/2 - 200, 560, 400};
    SDL_RenderFillRect(renderer, &card);
    SDL_SetRenderDrawColor(renderer, 50, 50, 70, 255);
    SDL_RenderDrawRect(renderer, &card);

    if (m_fontLarge)
        renderText(renderer, "Select Region", W/2 - 140, H/2 - 175, white, m_fontLarge);
    if (m_fontSmall)
        renderText(renderer, m_mod.name, W/2 - 100, H/2 - 130, grey, m_fontSmall);

    SDL_SetRenderDrawColor(renderer, 50, 50, 70, 255);
    SDL_RenderDrawLine(renderer, W/2 - 240, H/2 - 105, W/2 + 240, H/2 - 105);

    int ey = H/2 - 88;
    for (int i = 0; i < (int)m_entries.size(); i++) {
        bool sel = (i == m_selected);
        auto& e = m_entries[i];

        if (sel) {
            SDL_SetRenderDrawColor(renderer, 35, 35, 60, 255);
            SDL_Rect row = {W/2 - 240, ey - 6, 480, 44};
            SDL_RenderFillRect(renderer, &row);
            SDL_SetRenderDrawColor(renderer, 80, 180, 255, 255);
            SDL_Rect bar = {W/2 - 240, ey - 6, 4, 44};
            SDL_RenderFillRect(renderer, &bar);
        }

        SDL_Color regionColor = sel ? accent : white;
        SDL_Color idColor     = {100, 100, 130, 255};

        if (m_fontNormal)
            renderText(renderer, e.region, W/2 - 220, ey, regionColor, m_fontNormal);
        if (m_fontSmall)
            renderText(renderer, e.id, W/2 - 80, ey + 4, idColor, m_fontSmall);

        ey += 55;
    }

    if (m_fontSmall)
        renderText(renderer, "A: Install   B: Cancel", W/2 - 90, H/2 + 160, grey, m_fontSmall);
}

void RegionSelectScreen::renderText(SDL_Renderer* renderer, const std::string& text,
                                     int x, int y, SDL_Color color, TTF_Font* font) {
    if (!font || text.empty()) return;
    SDL_Surface* s = TTF_RenderUTF8_Blended(font, text.c_str(), color);
    if (!s) return;
    SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
    if (t) {
        SDL_Rect dst = {x, y, s->w, s->h};
        SDL_RenderCopy(renderer, t, nullptr, &dst);
        SDL_DestroyTexture(t);
    }
    SDL_FreeSurface(s);
}
