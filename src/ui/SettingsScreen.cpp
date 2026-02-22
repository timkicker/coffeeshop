#include "SettingsScreen.h"
#include "app/App.h"
#include "app/Paths.h"
#include "util/Logger.h"
#include "mods/InstalledScanner.h"
#include "net/DownloadQueue.h"
#include "util/ImageCache.h"

#include <SDL2/SDL.h>
#include <sys/statvfs.h>
#include <sys/stat.h>
#include <dirent.h>
#include <cstring>

static constexpr const char* FONT_PATH = "/vol/content/fonts/Roboto-Regular.ttf";

// Get directory size in bytes
static uint64_t dirSize(const std::string& path) {
    uint64_t total = 0;
    DIR* d = opendir(path.c_str());
    if (!d) return 0;
    struct dirent* e;
    while ((e = readdir(d)) != nullptr) {
        std::string name = e->d_name;
        if (name == "." || name == "..") continue;
        std::string child = path + "/" + name;
        struct stat st;
        if (stat(child.c_str(), &st) == 0) {
            if (S_ISDIR(st.st_mode)) total += dirSize(child);
            else total += st.st_size;
        }
    }
    closedir(d);
    return total;
}

static bool pathExists(const std::string& path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0;
}

static std::string fmtSize(uint64_t bytes) {
    char buf[32];
    if (bytes < 1024) snprintf(buf, sizeof(buf), "%llu B", (unsigned long long)bytes);
    else if (bytes < 1024*1024) snprintf(buf, sizeof(buf), "%.1f KB", bytes/1024.0);
    else snprintf(buf, sizeof(buf), "%.1f MB", bytes/(1024.0*1024.0));
    return buf;
}

static std::string freeSpace(const std::string& path) {
    struct statvfs sv;
    if (statvfs(path.c_str(), &sv) != 0) return "Unknown";
    uint64_t free = (uint64_t)sv.f_bavail * sv.f_frsize;
    return fmtSize(free) + " free";
}

SettingsScreen::SettingsScreen(App* app, const Config& config)
    : Screen(app), m_config(config) {}

SettingsScreen::~SettingsScreen() {
    if (m_fontLarge)  TTF_CloseFont(m_fontLarge);
    if (m_fontNormal) TTF_CloseFont(m_fontNormal);
    if (m_fontSmall)  TTF_CloseFont(m_fontSmall);
    if (m_fontTiny)   TTF_CloseFont(m_fontTiny);
}

void SettingsScreen::onEnter() {
    m_fontLarge  = TTF_OpenFont(FONT_PATH, 28);
    m_fontNormal = TTF_OpenFont(FONT_PATH, 20);
    m_fontSmall  = TTF_OpenFont(FONT_PATH, 16);
    m_fontTiny   = TTF_OpenFont(FONT_PATH, 13);
    buildItems();
}

void SettingsScreen::onExit() {}

void SettingsScreen::buildItems() {
    m_items.clear();

    auto header = [&](const std::string& title) {
        Item h; h.label = title; h.isHeader = true;
        m_items.push_back(h);
    };
    auto info = [&](const std::string& label, const std::string& value) {
        Item i; i.label = label; i.value = value;
        m_items.push_back(i);
    };
    auto button = [&](const std::string& label) {
        Item b; b.label = label; b.isButton = true;
        m_items.push_back(b);
    };

    // --- Repos ---
    header("Repos");
    for (auto& url : m_config.repos)
        info(url, pathExists(Paths::sdcafiineBase()) ? "" : ""); // status checked async would be better, skip for now
    info("Config file", Paths::configFile());

    // --- System ---
    header("System");
    info("SDCafiine folder", pathExists(Paths::sdcafiineBase()) ? "Found" : "Not found");
    info("SD card", pathExists(Paths::sdRoot()) ? "Mounted" : "Not detected");
    info("Free space", freeSpace(Paths::sdRoot()));

    auto mods = InstalledScanner::scan();
    int active = 0, inactive = 0;
    for (auto& m : mods) { if (m.active) active++; else inactive++; }
    info("Installed mods", std::to_string(active) + " active / " + std::to_string(inactive) + " inactive");

    // --- Cache ---
    header("Cache");
    uint64_t imgSize   = dirSize(Paths::cacheDir() + "/images");
    uint64_t totalSize = dirSize(Paths::cacheDir());
    info("Image cache", fmtSize(imgSize));
    info("Total cache", fmtSize(totalSize));
    button("Clear image cache");
    button("Clear all cache");

    // --- Logs ---
    header("Logs");
    info("Log file", Logger::get().path().empty() ? "Not initialized" : Logger::get().path());
    button("View log");

    // --- App ---
    header("App");
#ifdef APP_VERSION
    info("Version", APP_VERSION);
#else
    info("Version", "unknown");
#endif
#if BUILD_HW
    info("Build", "Hardware");
#else
    info("Build", "Cemu / Debug");
#endif
    info("Author", "Tim Kicker");
    info("GitHub", "github.com/timkicker/cupstore");
}

void SettingsScreen::handleInput(const Input& input) {
    // Log viewer mode
    if (m_showLog) {
        auto& lines = Logger::get().lines();
        if (input.up)   m_logScroll = std::max(0, m_logScroll - 1);
        if (input.down) m_logScroll = std::min(std::max(0, (int)lines.size() - 20), m_logScroll + 1);
        if (input.b)    m_showLog = false;
        return;
    }

    // Skip headers when navigating
    auto isSelectable = [&](int idx) {
        return idx >= 0 && idx < (int)m_items.size() && !m_items[idx].isHeader;
    };

    if (input.b) { m_app->popScreen(); return; }

    if (input.up) {
        int next = m_selectedIdx - 1;
        while (next >= 0 && !isSelectable(next)) next--;
        if (next >= 0) m_selectedIdx = next;
    }
    if (input.down) {
        int next = m_selectedIdx + 1;
        while (next < (int)m_items.size() && !isSelectable(next)) next++;
        if (next < (int)m_items.size()) m_selectedIdx = next;
    }

    // Scroll to keep selected visible
    if (m_selectedIdx < m_scrollOffset) m_scrollOffset = m_selectedIdx;
    if (m_selectedIdx >= m_scrollOffset + 14) m_scrollOffset = m_selectedIdx - 13;

    if (input.a && m_items[m_selectedIdx].isButton) {
        std::string label = m_items[m_selectedIdx].label;
        if (label == "Clear image cache") {
            std::string imgDir = Paths::cacheDir() + "/images";
            DIR* d = opendir(imgDir.c_str());
            if (d) {
                struct dirent* e;
                while ((e = readdir(d)) != nullptr) {
                    std::string name = e->d_name;
                    if (name == "." || name == "..") continue;
                    remove((imgDir + "/" + name).c_str());
                }
                closedir(d);
            }
            ImageCache::get().clear(nullptr); // clear texture cache too
            LOG_INFO("Settings: image cache cleared");
            buildItems(); // refresh sizes
        }
        else if (label == "Clear all cache") {
            std::string cacheDir = Paths::cacheDir();
            DIR* d = opendir(cacheDir.c_str());
            if (d) {
                struct dirent* e;
                while ((e = readdir(d)) != nullptr) {
                    std::string name = e->d_name;
                    if (name == "." || name == "..") continue;
                    remove((cacheDir + "/" + name).c_str());
                }
                closedir(d);
            }
            ImageCache::get().clear(nullptr);
            LOG_INFO("Settings: full cache cleared");
            buildItems();
        }
        else if (label == "View log") {
            m_showLog   = true;
            m_logScroll = std::max(0, (int)Logger::get().lines().size() - 20);
        }
    }
}

void SettingsScreen::update() {}

void SettingsScreen::render(SDL_Renderer* renderer) {
    const int W = m_app->screenWidth();
    const int H = m_app->screenHeight();

    SDL_SetRenderDrawColor(renderer, 12, 12, 20, 255);
    SDL_Rect bg = {0, 0, W, H};
    SDL_RenderFillRect(renderer, &bg);

    // Sidebar
    const int SIDEBAR_W = 130;
    SDL_SetRenderDrawColor(renderer, 18, 18, 30, 255);
    SDL_Rect sidebar = {0, 0, SIDEBAR_W, H};
    SDL_RenderFillRect(renderer, &sidebar);
    SDL_SetRenderDrawColor(renderer, 40, 40, 60, 255);
    SDL_RenderDrawLine(renderer, SIDEBAR_W, 0, SIDEBAR_W, H);

    if (m_fontNormal) renderText(renderer, "CupStore", 12, 14, {255,255,255,255}, m_fontNormal);
    SDL_SetRenderDrawColor(renderer, 40, 40, 60, 255);
    SDL_RenderDrawLine(renderer, 8, 42, SIDEBAR_W-8, 42);

    int sy = 54;
    for (auto& [label, active] : std::vector<std::pair<std::string,bool>>{
        {"Browse",    false},
        {"Installed", false},
        {"Settings",  true}
    }) {
        if (active) {
            SDL_SetRenderDrawColor(renderer, 30, 60, 100, 255);
            SDL_Rect sel = {0, sy-4, SIDEBAR_W, 30};
            SDL_RenderFillRect(renderer, &sel);
            SDL_SetRenderDrawColor(renderer, 80, 180, 255, 255);
            SDL_Rect bar = {0, sy-4, 3, 30};
            SDL_RenderFillRect(renderer, &bar);
        }
        SDL_Color col = active ? SDL_Color{255,255,255,255} : SDL_Color{140,140,170,255};
        if (m_fontSmall) renderText(renderer, label, 14, sy, col, m_fontSmall);
        sy += 36;
    }

    // Content area
    const int CX = SIDEBAR_W + 10;

    // Top bar
    SDL_SetRenderDrawColor(renderer, 20, 20, 33, 255);
    SDL_Rect topbar = {SIDEBAR_W, 0, W-SIDEBAR_W, 50};
    SDL_RenderFillRect(renderer, &topbar);
    SDL_SetRenderDrawColor(renderer, 45, 45, 65, 255);
    SDL_RenderDrawLine(renderer, SIDEBAR_W, 50, W, 50);
    if (m_fontSmall) renderText(renderer, "B  Back", SIDEBAR_W+16, 14, {80,180,255,255}, m_fontSmall);
    if (m_fontLarge) renderText(renderer, "Settings", SIDEBAR_W+110, 10, {255,255,255,255}, m_fontLarge);

    // Log viewer overlay
    if (m_showLog) {
        SDL_SetRenderDrawColor(renderer, 8, 8, 14, 245);
        SDL_Rect logOverlay = {SIDEBAR_W, 0, W-SIDEBAR_W, H};
        SDL_RenderFillRect(renderer, &logOverlay);
        if (m_fontSmall) renderText(renderer, "Log Viewer   B: Close   Up/Down: scroll", SIDEBAR_W+20, 14, {80,180,255,255}, m_fontSmall);
        SDL_SetRenderDrawColor(renderer, 30, 30, 50, 255);
        SDL_Rect logBg = {SIDEBAR_W+10, 55, W-SIDEBAR_W-20, H-65};
        SDL_RenderFillRect(renderer, &logBg);

        auto& lines = Logger::get().lines();
        int visibleLines = (H - 80) / 16;
        int y = 60;
        for (int i = m_logScroll; i < (int)lines.size() && i < m_logScroll + visibleLines; i++) {
            SDL_Color col = {180, 180, 200, 255};
            if (lines[i].find("ERROR") != std::string::npos) col = {220, 70, 70, 255};
            else if (lines[i].find("WARN") != std::string::npos) col = {220, 180, 50, 255};
            if (m_fontTiny) renderText(renderer, lines[i], SIDEBAR_W+18, y, col, m_fontTiny);
            y += 16;
        }
        if (m_fontTiny) {
            std::string pg = std::to_string(m_logScroll+1) + "/" + std::to_string(std::max(1,(int)lines.size()));
            renderText(renderer, pg, W-80, H-20, {80,80,110,255}, m_fontTiny);
        }
        return;
    }

    // Items list
    const int ITEM_H = 36;
    const int LIST_Y = 58;
    int y = LIST_Y;

    const int CX2 = 140; // sidebar width + padding
    for (int i = m_scrollOffset; i < (int)m_items.size() && y < H-40; i++) {
        auto& item = m_items[i];
        bool selected = (i == m_selectedIdx);

        if (item.isHeader) {
            y += 6;
            if (m_fontSmall) renderText(renderer, item.label, CX2, y, {80,180,255,255}, m_fontSmall);
            y += 20;
            SDL_SetRenderDrawColor(renderer, 40, 40, 65, 255);
            SDL_RenderDrawLine(renderer, CX2, y, W-20, y);
            y += 6;
            continue;
        }

        // Row background
        if (selected) {
            SDL_SetRenderDrawColor(renderer, 30, 50, 80, 255);
            SDL_Rect row = {CX2-4, y, W-CX2, ITEM_H-4};
            SDL_RenderFillRect(renderer, &row);
            SDL_SetRenderDrawColor(renderer, 80, 180, 255, 255);
            SDL_RenderDrawRect(renderer, &row);
        }

        SDL_Color labelCol = item.isButton
            ? SDL_Color{100, 200, 120, 255}
            : SDL_Color{210, 210, 230, 255};

        if (m_fontSmall) renderText(renderer, item.label, CX2+4, y+8, labelCol, m_fontSmall);
        if (!item.value.empty() && m_fontSmall)
            renderText(renderer, item.value, W/2+40, y+8, {130,130,160,255}, m_fontSmall);

        y += ITEM_H;
    }

    // Bottom hint
    if (m_fontTiny) {
        bool onButton = m_selectedIdx < (int)m_items.size() && m_items[m_selectedIdx].isButton;
        std::string hint = onButton ? "A: Execute   B: Back   Up/Down: navigate"
                                    : "B: Back   Up/Down: navigate";
        renderText(renderer, hint, W/2-180, H-22, {70,70,95,255}, m_fontTiny);
    }
}

void SettingsScreen::renderText(SDL_Renderer* renderer, const std::string& text,
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
