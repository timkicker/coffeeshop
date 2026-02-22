#include "DetailScreen.h"
#include "ui/RegionSelectScreen.h"
#include "mods/InstallHelper.h"
#include "mods/InstallChecker.h"
#include "mods/InstalledScanner.h"
#include "net/DownloadQueue.h"
#include "audio/AudioManager.h"
#include "app/App.h"
#include "util/ImageCache.h"
#include <cstdio>
#include <algorithm>

static constexpr const char* FONT_PATH = "/vol/content/fonts/Roboto-Regular.ttf";

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

    m_installStatus = InstallChecker::check(m_mod.id, m_mod.version, m_titleIds);

    if (!m_mod.thumbnail.empty()) ImageCache::get().request(m_mod.thumbnail);
    for (auto& s : m_mod.screenshots) ImageCache::get().request(s);
}

void DetailScreen::onExit() {}

void DetailScreen::handleInput(const Input& input) {
    if (m_confirmUninstall) {
        if (input.a) {
            auto mods = InstalledScanner::scan();
            for (auto& inst : mods) {
                if (inst.id == m_mod.id) {
                    InstalledScanner::remove(inst);
                    break;
                }
            }
            m_app->popScreen();
        }
        if (input.b) m_confirmUninstall = false;
        return;
    }

    if (input.b) { m_app->popScreen(); return; }

    if (input.y && m_installStatus.installed) {
        m_confirmUninstall = true;
        return;
    }

    if (input.a && !m_titleIds.empty()) {
        if (m_installStatus.installed && !m_installStatus.updateAvail) return;
        auto entries = InstallHelper::detectInstalled(m_titleIds);
        if (entries.size() == 1) {
            AudioManager::get().playSound(SoundId::DownloadStart);
            DownloadQueue::get().enqueue(m_mod, entries[0].id);
            m_app->popScreen();
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

    // Top bar
    SDL_SetRenderDrawColor(renderer, 20, 20, 33, 255);
    SDL_Rect topbar = {0, 0, W, 50};
    SDL_RenderFillRect(renderer, &topbar);
    SDL_SetRenderDrawColor(renderer, 45, 45, 65, 255);
    SDL_RenderDrawLine(renderer, 0, 50, W, 50);
    if (m_fontSmall) renderText(renderer, "B  Back", 16, 14, accent, m_fontSmall);
    if (m_fontLarge) renderText(renderer, m_mod.name, 110, 9, white, m_fontLarge);

    // Type badge
    bool isModpack = (m_mod.type == "modpack");
    SDL_Color badgeBg = isModpack ? SDL_Color{110,50,170,255} : SDL_Color{35,90,170,255};
    int titleW = 0;
    if (m_fontLarge) TTF_SizeText(m_fontLarge, m_mod.name.c_str(), &titleW, nullptr);
    SDL_Rect badge = {110 + titleW + 14, 14, isModpack ? 76 : 50, 22};
    SDL_SetRenderDrawColor(renderer, badgeBg.r, badgeBg.g, badgeBg.b, 255);
    SDL_RenderFillRect(renderer, &badge);
    if (m_fontTiny)
        renderText(renderer, isModpack ? "MODPACK" : "MOD", badge.x+7, badge.y+4, white, m_fontTiny);

    const int MARGIN    = 24;
    const int SPLIT     = (int)(W * 0.54f);
    const int CONTENT_Y = 62;
    const int THUMB_W   = SPLIT - MARGIN * 2;
    const int THUMB_H   = (int)(THUMB_W * 9.0f / 16.0f);
    const int THUMB_X   = MARGIN;
    const int THUMB_Y   = CONTENT_Y;

    // Thumbnail / screenshot
    SDL_Rect thumb = {THUMB_X, THUMB_Y, THUMB_W, THUMB_H};
    {
        std::string imgUrl;
        if (!m_mod.screenshots.empty()) imgUrl = m_mod.screenshots[m_screenshotIndex];
        else if (!m_mod.thumbnail.empty()) imgUrl = m_mod.thumbnail;
        SDL_Texture* imgTex = imgUrl.empty() ? nullptr : ImageCache::get().texture(imgUrl, renderer);
        if (imgTex) {
            SDL_RenderCopy(renderer, imgTex, nullptr, &thumb);
        } else {
            SDL_SetRenderDrawColor(renderer, 25, 25, 42, 255);
            SDL_RenderFillRect(renderer, &thumb);
            SDL_SetRenderDrawColor(renderer, 48, 48, 72, 255);
            SDL_RenderDrawRect(renderer, &thumb);
        }
    }
    if (!m_mod.screenshots.empty() && m_fontTiny) {
        std::string ind = std::to_string(m_screenshotIndex+1) + " / "
                        + std::to_string(m_mod.screenshots.size());
        renderText(renderer, ind,                THUMB_X+THUMB_W/2-18, THUMB_Y+THUMB_H-22, grey, m_fontTiny);
        renderText(renderer, "< Left / Right >", THUMB_X+THUMB_W/2-52, THUMB_Y+THUMB_H+6,  dim,  m_fontTiny);
    }

    // Right column
    const int RX = SPLIT + MARGIN;
    const int RW = W - RX - MARGIN;
    int ry = CONTENT_Y;

    auto label = [&](const char* l) {
        if (m_fontTiny) renderText(renderer, l, RX, ry, grey, m_fontTiny);
        ry += 16;
    };
    auto value = [&](const std::string& v) {
        if (m_fontNormal) renderText(renderer, v, RX, ry, white, m_fontNormal);
        ry += 28;
    };
    auto smallvalue = [&](const std::string& v) {
        if (m_fontSmall) renderText(renderer, v, RX, ry, white, m_fontSmall);
        ry += 22;
    };

    label("Game");    value(m_gameName);
    label("Author");  value(m_mod.author);
    label("Version"); value(m_mod.version);

    if (!m_mod.releaseDate.empty()) { label("Released"); smallvalue(m_mod.releaseDate); }
    if (!m_mod.license.empty())     { label("License");  smallvalue(m_mod.license); }
    if (m_mod.fileSize > 0) {
        char szBuf[32];
        if (m_mod.fileSize < 1024*1024)
            snprintf(szBuf, sizeof(szBuf), "%.1f KB", m_mod.fileSize/1024.0);
        else
            snprintf(szBuf, sizeof(szBuf), "%.1f MB", m_mod.fileSize/(1024.0*1024.0));
        label("Size"); smallvalue(szBuf);
    }
    if (!m_mod.requirements.empty()) {
        std::string req;
        for (size_t i = 0; i < m_mod.requirements.size(); i++) {
            if (i) req += ", ";
            req += m_mod.requirements[i];
        }
        label("Requires"); smallvalue(req);
    }
    if (!m_mod.tags.empty()) {
        std::string tagStr;
        for (auto& t : m_mod.tags) tagStr += "#" + t + "  ";
        label("Tags");
        if (m_fontTiny) { renderText(renderer, tagStr, RX, ry, {100,160,220,255}, m_fontTiny); ry += 20; }
    }

    // Install status badge
    if (m_installStatus.installed) {
        ry += 4;
        if (m_installStatus.updateAvail) {
            SDL_SetRenderDrawColor(renderer, 180, 110, 20, 255);
            SDL_Rect pill = {RX, ry, 210, 22};
            SDL_RenderFillRect(renderer, &pill);
            if (m_fontTiny)
                renderText(renderer, "UPDATE  v" + m_installStatus.installedVersion
                           + " -> v" + m_mod.version, RX+6, ry+4, white, m_fontTiny);
        } else {
            SDL_SetRenderDrawColor(renderer, 30, 100, 50, 255);
            SDL_Rect pill = {RX, ry, 160, 22};
            SDL_RenderFillRect(renderer, &pill);
            if (m_fontTiny)
                renderText(renderer, "INSTALLED  v" + m_installStatus.installedVersion,
                           RX+6, ry+4, white, m_fontTiny);
        }
        ry += 30;
    }

    if (!m_mod.includes.empty()) {
        std::string inc;
        for (size_t i = 0; i < m_mod.includes.size(); i++) {
            if (i) inc += ", ";
            inc += m_mod.includes[i];
        }
        label("Includes"); smallvalue(inc);
    }

    ry += 6;
    SDL_SetRenderDrawColor(renderer, 38, 38, 58, 255);
    SDL_RenderDrawLine(renderer, RX, ry, RX+RW, ry);
    ry += 12;

    label("Description");
    if (!m_mod.description.empty())
        renderWrappedText(renderer, m_mod.description, RX, ry, RW, {195,195,218,255}, m_fontSmall);

    if (!m_mod.changelog.empty()) {
        ry += TTF_FontLineSkip(m_fontSmall) * 3 + 8;
        SDL_SetRenderDrawColor(renderer, 38, 38, 58, 255);
        SDL_RenderDrawLine(renderer, RX, ry, RX+RW, ry);
        ry += 8;
        label("Changelog");
        renderWrappedText(renderer, m_mod.changelog, RX, ry, RW, {160,180,160,255}, m_fontTiny);
    }

    // Bottom bar
    SDL_SetRenderDrawColor(renderer, 20, 20, 33, 255);
    SDL_Rect bottombar = {0, H-56, W, 56};
    SDL_RenderFillRect(renderer, &bottombar);
    SDL_SetRenderDrawColor(renderer, 45, 45, 65, 255);
    SDL_RenderDrawLine(renderer, 0, H-56, W, H-56);

    // Install button
    bool canInstall = !m_installStatus.installed || m_installStatus.updateAvail;
    SDL_Color btnBg  = canInstall ? SDL_Color{28,110,55,255}  : SDL_Color{40,40,60,255};
    SDL_Color btnBdr = canInstall ? SDL_Color{55,190,95,255}  : SDL_Color{70,70,90,255};
    std::string btnLabel = m_installStatus.installed
        ? (m_installStatus.updateAvail ? "A: Update" : "Installed")
        : "A: Download & Install";
    SDL_SetRenderDrawColor(renderer, btnBg.r, btnBg.g, btnBg.b, 255);
    SDL_Rect dlBtn = {MARGIN, H-44, 300, 34};
    SDL_RenderFillRect(renderer, &dlBtn);
    SDL_SetRenderDrawColor(renderer, btnBdr.r, btnBdr.g, btnBdr.b, 255);
    SDL_RenderDrawRect(renderer, &dlBtn);
    SDL_Color btnTxt = canInstall ? white : grey;
    if (m_fontSmall) renderText(renderer, btnLabel, MARGIN+12, H-38, btnTxt, m_fontSmall);

    // Uninstall button (only if installed)
    if (m_installStatus.installed) {
        SDL_SetRenderDrawColor(renderer, 80, 25, 25, 255);
        SDL_Rect unBtn = {W-200, H-44, 180, 34};
        SDL_RenderFillRect(renderer, &unBtn);
        SDL_SetRenderDrawColor(renderer, 180, 60, 60, 255);
        SDL_RenderDrawRect(renderer, &unBtn);
        if (m_fontSmall) renderText(renderer, "Y: Uninstall", W-188, H-38, {220,100,100,255}, m_fontSmall);
    }

    // Confirm uninstall overlay
    if (m_confirmUninstall) {
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 180);
        SDL_RenderFillRect(renderer, &fullbg);
        SDL_SetRenderDrawColor(renderer, 60, 20, 20, 255);
        SDL_Rect card = {W/2-200, H/2-50, 400, 100};
        SDL_RenderFillRect(renderer, &card);
        SDL_SetRenderDrawColor(renderer, 180, 60, 60, 255);
        SDL_RenderDrawRect(renderer, &card);
        if (m_fontSmall)  renderText(renderer, "Uninstall " + m_mod.name + "?", W/2-160, H/2-34, white, m_fontSmall);
        if (m_fontNormal) {
            renderText(renderer, "A: Confirm", W/2-160, H/2+4, {220,100,100,255}, m_fontNormal);
            renderText(renderer, "B: Cancel",  W/2+30,  H/2+4, grey, m_fontNormal);
        }
    }
}

void DetailScreen::renderText(SDL_Renderer* renderer, const std::string& text,
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
