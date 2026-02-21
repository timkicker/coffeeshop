#include "DetailScreen.h"
#include "net/DownloadQueue.h"
#include "ui/RegionSelectScreen.h"
#include "ui/RegionSelectScreen.h"
#include "mods/InstallHelper.h"
#include "app/App.h"

static constexpr const char* FONT_PATH = "/vol/content/Roboto-Regular.ttf";

DetailScreen::DetailScreen(App* app, const Mod& mod, const std::string& gameName,
                           const std::vector<std::string>& titleIds)
    : Screen(app), m_mod(mod), m_gameName(gameName), m_titleIds(titleIds) {}

DetailScreen::~DetailScreen() {
    if (m_fontLarge)  TTF_CloseFont(m_fontLarge);
    if (m_fontNormal) TTF_CloseFont(m_fontNormal);
    if (m_fontSmall)  TTF_CloseFont(m_fontSmall);
    if (m_fontTiny)   TTF_CloseFont(m_fontTiny);
}

void DetailScreen::onEnter() {
    m_fontLarge  = TTF_OpenFont(FONT_PATH, 32);
    m_fontNormal = TTF_OpenFont(FONT_PATH, 22);
    m_fontSmall  = TTF_OpenFont(FONT_PATH, 17);
    m_fontTiny   = TTF_OpenFont(FONT_PATH, 13);
}

void DetailScreen::onExit() {}

void DetailScreen::handleInput(const Input& input) {
    if (input.b) m_app->popScreen();

    if (input.a && !m_titleIds.empty()) {
        auto entries = InstallHelper::detectInstalled(m_titleIds);
        if (entries.size() == 1) {
            DownloadQueue::get().enqueue(m_mod, entries[0].id);
            m_app->popScreen(); // back to browse
        } else {
            m_app->pushScreen(std::make_unique<RegionSelectScreen>(
                m_app, m_mod, entries));
        }
    }

    if (!m_mod.screenshots.empty()) {
        if (input.left)
            m_screenshotIndex = (m_screenshotIndex - 1 + (int)m_mod.screenshots.size())
                                % (int)m_mod.screenshots.size();
        if (input.right)
            m_screenshotIndex = (m_screenshotIndex + 1) % (int)m_mod.screenshots.size();
    }
}

void DetailScreen::update() {}

void DetailScreen::render(SDL_Renderer* renderer) {
    const int W = m_app->screenWidth();
    const int H = m_app->screenHeight();

    SDL_Color white  = {255, 255, 255, 255};
    SDL_Color grey   = {130, 130, 155, 255};
    SDL_Color accent = { 80, 180, 255, 255};
    SDL_Color dim    = { 70,  70,  95, 255};

    SDL_SetRenderDrawColor(renderer, 12, 12, 20, 255);
    SDL_Rect fullbg = {0, 0, W, H};
    SDL_RenderFillRect(renderer, &fullbg);

    SDL_SetRenderDrawColor(renderer, 20, 20, 33, 255);
    SDL_Rect topbar = {0, 0, W, 50};
    SDL_RenderFillRect(renderer, &topbar);
    SDL_SetRenderDrawColor(renderer, 45, 45, 65, 255);
    SDL_RenderDrawLine(renderer, 0, 50, W, 50);

    if (m_fontSmall) renderText(renderer, "B  Back", 16, 14, accent, m_fontSmall);
    if (m_fontLarge) renderText(renderer, m_mod.name, 110, 9, white, m_fontLarge);

    bool isModpack = (m_mod.type == "modpack");
    SDL_Color badgeBg = isModpack ? SDL_Color{110,50,170,255} : SDL_Color{35,90,170,255};
    int titleW = 0;
    if (m_fontLarge) TTF_SizeText(m_fontLarge, m_mod.name.c_str(), &titleW, nullptr);
    SDL_Rect badge = {110 + titleW + 14, 14, isModpack ? 76 : 50, 22};
    SDL_SetRenderDrawColor(renderer, badgeBg.r, badgeBg.g, badgeBg.b, 255);
    SDL_RenderFillRect(renderer, &badge);
    if (m_fontTiny)
        renderText(renderer, isModpack ? "MODPACK" : "MOD", badge.x+7, badge.y+4, white, m_fontTiny);

    const int MARGIN   = 24;
    const int SPLIT    = (int)(W * 0.54f);
    const int CONTENT_Y = 62;
    const int THUMB_W = SPLIT - MARGIN * 2;
    const int THUMB_H = (int)(THUMB_W * 9.0f / 16.0f);
    const int THUMB_X = MARGIN;
    const int THUMB_Y = CONTENT_Y;

    SDL_SetRenderDrawColor(renderer, 25, 25, 42, 255);
    SDL_Rect thumb = {THUMB_X, THUMB_Y, THUMB_W, THUMB_H};
    SDL_RenderFillRect(renderer, &thumb);
    SDL_SetRenderDrawColor(renderer, 48, 48, 72, 255);
    SDL_RenderDrawRect(renderer, &thumb);

    int cx = THUMB_X + THUMB_W/2;
    int cy = THUMB_Y + THUMB_H/2;
    SDL_SetRenderDrawColor(renderer, 55, 55, 88, 255);
    SDL_RenderDrawLine(renderer, cx-28, cy, cx+28, cy);
    SDL_RenderDrawLine(renderer, cx, cy-28, cx, cy+28);

    if (!m_mod.screenshots.empty() && m_fontTiny) {
        std::string ind = std::to_string(m_screenshotIndex+1) + " / "
                        + std::to_string(m_mod.screenshots.size());
        renderText(renderer, ind,              THUMB_X+THUMB_W/2-18, THUMB_Y+THUMB_H-22, grey, m_fontTiny);
        renderText(renderer, "< Left / Right >", THUMB_X+THUMB_W/2-52, THUMB_Y+THUMB_H+6, dim, m_fontTiny);
    }

    const int RX = SPLIT + MARGIN;
    const int RW = W - RX - MARGIN;
    int ry = CONTENT_Y;

    auto label = [&](const std::string& l) {
        if (m_fontTiny) renderText(renderer, l, RX, ry, grey, m_fontTiny);
        ry += 16;
    };
    auto value = [&](const std::string& v) {
        if (m_fontNormal) renderText(renderer, v, RX, ry, white, m_fontNormal);
        ry += 30;
    };

    label("Game");    value(m_gameName);
    label("Author");  value(m_mod.author);
    label("Version"); value(m_mod.version);

    if (!m_mod.includes.empty()) {
        std::string inc;
        for (auto& s : m_mod.includes) inc += s + ", ";
        if (inc.size() > 2) inc = inc.substr(0, inc.size()-2);
        label("Includes"); value(inc);
    }

    ry += 6;
    SDL_SetRenderDrawColor(renderer, 38, 38, 58, 255);
    SDL_RenderDrawLine(renderer, RX, ry, RX+RW, ry);
    ry += 12;

    label("Description");
    if (!m_mod.description.empty())
        renderWrappedText(renderer, m_mod.description, RX, ry, RW, {195,195,218,255}, m_fontSmall);

    SDL_SetRenderDrawColor(renderer, 20, 20, 33, 255);
    SDL_Rect bottombar = {0, H-56, W, 56};
    SDL_RenderFillRect(renderer, &bottombar);
    SDL_SetRenderDrawColor(renderer, 45, 45, 65, 255);
    SDL_RenderDrawLine(renderer, 0, H-56, W, H-56);

    SDL_SetRenderDrawColor(renderer, 28, 110, 55, 255);
    SDL_Rect dlBtn = {W/2-130, H-44, 260, 34};
    SDL_RenderFillRect(renderer, &dlBtn);
    SDL_SetRenderDrawColor(renderer, 55, 190, 95, 255);
    SDL_RenderDrawRect(renderer, &dlBtn);
    if (m_fontSmall)
        renderText(renderer, "A: Download & Install", W/2-108, H-38, white, m_fontSmall);
}

void DetailScreen::renderText(SDL_Renderer* renderer, const std::string& text,
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

void DetailScreen::renderWrappedText(SDL_Renderer* renderer, const std::string& text,
                                      int x, int y, int maxW, SDL_Color color, TTF_Font* font) {
    if (!font || text.empty()) return;
    int lineH = TTF_FontLineSkip(font);
    std::string word, line;
    int lineY = y;

    auto flush = [&]() {
        if (!line.empty()) {
            renderText(renderer, line, x, lineY, color, font);
            lineY += lineH;
            line.clear();
        }
    };

    for (size_t i = 0; i <= text.size(); i++) {
        char c = (i < text.size()) ? text[i] : ' ';
        if (c == ' ' || i == text.size()) {
            std::string test = line.empty() ? word : line + " " + word;
            int tw = 0;
            TTF_SizeText(font, test.c_str(), &tw, nullptr);
            if (tw > maxW && !line.empty()) { flush(); line = word; }
            else line = test;
            word.clear();
        } else {
            word += c;
        }
    }
    flush();
}
