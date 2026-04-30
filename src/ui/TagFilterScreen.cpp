#include "TagFilterScreen.h"
#include "app/App.h"
#include "audio/AudioManager.h"
#include "util/TextCache.h"

#include <algorithm>

static constexpr const char* FONT_PATH = "/vol/content/fonts/Roboto-Regular.ttf";

TagFilterScreen::TagFilterScreen(App* app,
                                 std::vector<std::string> available,
                                 std::set<std::string> selected,
                                 Callback onClose)
    : Screen(app),
      m_tags(std::move(available)),
      m_selected(std::move(selected)),
      m_onClose(std::move(onClose)) {}

void TagFilterScreen::onEnter() {
    m_fontLarge  = TTF_OpenFont(FONT_PATH, 28);
    m_fontNormal = TTF_OpenFont(FONT_PATH, 22);
    m_fontSmall  = TTF_OpenFont(FONT_PATH, 16);
}

void TagFilterScreen::onExit() {
    if (m_fontLarge)  TTF_CloseFont(m_fontLarge);
    if (m_fontNormal) TTF_CloseFont(m_fontNormal);
    if (m_fontSmall)  TTF_CloseFont(m_fontSmall);
}

void TagFilterScreen::handleInput(const Input& input) {
    if (input.b) {
        if (m_onClose) m_onClose(m_selected);
        m_app->popScreen();
        return;
    }
    if (m_tags.empty()) return;

    if (input.up && m_cursor > 0) {
        m_cursor--;
        AudioManager::get().playSound(SoundId::Navigate);
    }
    if (input.down && m_cursor + 1 < (int)m_tags.size()) {
        m_cursor++;
        AudioManager::get().playSound(SoundId::Navigate);
    }
    if (input.a) {
        const std::string& t = m_tags[m_cursor];
        if (m_selected.count(t)) m_selected.erase(t);
        else                     m_selected.insert(t);
        AudioManager::get().playSound(SoundId::Navigate);
    }
    if (input.x) {
        // Quick clear all
        m_selected.clear();
        AudioManager::get().playSound(SoundId::Navigate);
    }

    // Scroll to keep cursor visible (10 visible rows)
    if (m_cursor < m_scroll) m_scroll = m_cursor;
    if (m_cursor >= m_scroll + 10) m_scroll = m_cursor - 9;
}

void TagFilterScreen::update() {}

void TagFilterScreen::render(SDL_Renderer* renderer) {
    const int W = m_app->screenWidth();
    const int H = m_app->screenHeight();

    // Dim background
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 200);
    SDL_Rect dim = {0, 0, W, H};
    SDL_RenderFillRect(renderer, &dim);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

    // Panel
    int pw = 480, ph = 460;
    int px = (W - pw) / 2, py = (H - ph) / 2;
    SDL_SetRenderDrawColor(renderer, 22, 22, 35, 255);
    SDL_Rect panel = {px, py, pw, ph};
    SDL_RenderFillRect(renderer, &panel);
    SDL_SetRenderDrawColor(renderer, 70, 90, 130, 255);
    SDL_RenderDrawRect(renderer, &panel);

    SDL_Color white  = {255, 255, 255, 255};
    SDL_Color grey   = {150, 150, 175, 255};
    SDL_Color accent = { 80, 180, 255, 255};

    if (m_fontLarge) renderText(renderer, "Filter by tag", px + 20, py + 16, white, m_fontLarge);

    if (m_tags.empty()) {
        if (m_fontNormal)
            renderText(renderer, "No tags in this repo.", px + 20, py + 70, grey, m_fontNormal);
    } else {
        int rowY = py + 60;
        const int kVisible = 10;
        for (int i = m_scroll; i < (int)m_tags.size() && i < m_scroll + kVisible; i++) {
            const auto& tag = m_tags[i];
            bool selected = (m_selected.count(tag) > 0);
            bool cursor   = (i == m_cursor);

            if (cursor) {
                SDL_SetRenderDrawColor(renderer, 35, 50, 80, 255);
                SDL_Rect row = {px + 12, rowY - 2, pw - 24, 30};
                SDL_RenderFillRect(renderer, &row);
            }

            std::string check = selected ? "[x] " : "[ ] ";
            if (m_fontNormal)
                renderText(renderer, check + tag, px + 24, rowY,
                           selected ? accent : white, m_fontNormal);

            rowY += 32;
        }
    }

    if (m_fontSmall)
        renderText(renderer,
                   "A: toggle  X: clear all  B: done  (" +
                   std::to_string((int)m_selected.size()) + " selected)",
                   px + 20, py + ph - 30, grey, m_fontSmall);
}

void TagFilterScreen::renderText(SDL_Renderer* renderer, const std::string& text,
                                 int x, int y, SDL_Color color, TTF_Font* font) {
    if (!font || text.empty()) return;
    SDL_Texture* t = TextCache::get().texture(renderer, font, color, text);
    if (!t) return;
    int w = 0, h = 0;
    TextCache::get().sizeOf(t, &w, &h);
    SDL_Rect dst = {x, y, w, h};
    SDL_RenderCopy(renderer, t, nullptr, &dst);
}
