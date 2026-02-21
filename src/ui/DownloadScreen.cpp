#include "DownloadScreen.h"
#include "app/App.h"
#include "app/Paths.h"
#include "util/ScreenLog.h"
#include "util/Logger.h"

static constexpr const char* FONT_PATH = "/vol/content/Roboto-Regular.ttf";

DownloadScreen::DownloadScreen(App* app, const Mod& mod, const std::string& titleId)
    : Screen(app), m_mod(mod), m_titleId(titleId) {}

DownloadScreen::~DownloadScreen() {
    if (m_thread.joinable()) m_thread.join();
    if (m_fontLarge)  TTF_CloseFont(m_fontLarge);
    if (m_fontNormal) TTF_CloseFont(m_fontNormal);
    if (m_fontSmall)  TTF_CloseFont(m_fontSmall);
}

void DownloadScreen::onEnter() {
    m_fontLarge  = TTF_OpenFont(FONT_PATH, 32);
    m_fontNormal = TTF_OpenFont(FONT_PATH, 22);
    m_fontSmall  = TTF_OpenFont(FONT_PATH, 17);

    std::string tmpPath = Paths::cacheDir() + "/" + m_mod.id + ".zip";
    std::string destDir = Paths::sdcafiineBase() + "/" + m_titleId + "/" + m_mod.id;

    SLOG("tmpPath: " + tmpPath);
    SLOG("destDir: " + destDir);
    SLOG("cacheDir: " + Paths::cacheDir());
    SLOG("sdMounted: " + std::string(Paths::sdMounted ? "true" : "false"));

    m_thread = std::thread([this, tmpPath, destDir]() {
        m_dm.run(m_mod.download, tmpPath, destDir);
    });
}

void DownloadScreen::onExit() {
    if (m_thread.joinable()) m_thread.join();
}

void DownloadScreen::handleInput(const Input& input) {
    auto state = m_dm.state();
    if (input.b && (state == DownloadManager::State::Done ||
                    state == DownloadManager::State::Error)) {
        m_app->popScreen();
        m_app->popScreen();
    }
}

void DownloadScreen::update() {}

void DownloadScreen::render(SDL_Renderer* renderer) {
    const int W = m_app->screenWidth();
    const int H = m_app->screenHeight();

    SDL_Color white  = {255, 255, 255, 255};
    SDL_Color grey   = {130, 130, 155, 255};
    SDL_Color accent = { 80, 180, 255, 255};
    SDL_Color green  = { 55, 190,  95, 255};
    SDL_Color red    = {220,  70,  70, 255};
    SDL_Color yellow = {255, 220,  60, 255};

    SDL_SetRenderDrawColor(renderer, 12, 12, 20, 255);
    SDL_Rect bg = {0, 0, W, H};
    SDL_RenderFillRect(renderer, &bg);

    auto state    = m_dm.state();
    float progress = m_dm.progress();

    if (m_fontLarge)
        renderText(renderer, m_mod.name, W/2 - 200, 40, white, m_fontLarge);

    if (state == DownloadManager::State::Downloading) {
        if (m_fontNormal)
            renderText(renderer, "Downloading...", W/2 - 100, 110, grey, m_fontNormal);
        renderProgressBar(renderer, W/2 - 250, 150, 500, 28, progress);
        std::string pct = std::to_string((int)(progress * 100)) + "%";
        if (m_fontSmall)
            renderText(renderer, pct, W/2 - 18, 188, accent, m_fontSmall);
    } else if (state == DownloadManager::State::Extracting) {
        if (m_fontNormal)
            renderText(renderer, "Installing...", W/2 - 90, 110, grey, m_fontNormal);
        renderProgressBar(renderer, W/2 - 250, 150, 500, 28, 1.0f);
    } else if (state == DownloadManager::State::Done) {
        if (m_fontNormal)
            renderText(renderer, "Installed successfully!", W/2 - 150, 130, green, m_fontNormal);
        if (m_fontSmall)
            renderText(renderer, "B: Back to Browse", W/2 - 100, 170, grey, m_fontSmall);
    } else if (state == DownloadManager::State::Error) {
        if (m_fontNormal)
            renderText(renderer, "Installation failed.", W/2 - 130, 110, red, m_fontNormal);
        if (m_fontSmall) {
            renderText(renderer, m_dm.error(), 20, 145, grey, m_fontSmall);
            renderText(renderer, "B: Back", W/2 - 40, 175, grey, m_fontSmall);
        }
    } else {
        if (m_fontNormal)
            renderText(renderer, "Preparing...", W/2 - 80, 130, grey, m_fontNormal);
    }

    // Screen log overlay at bottom
    if (m_fontSmall) {
        auto lines = ScreenLog::get().lines();
        int ly = H - (int)lines.size() * 20 - 10;
        for (auto& l : lines) {
            renderText(renderer, l, 10, ly, yellow, m_fontSmall);
            ly += 20;
        }
    }
}

void DownloadScreen::renderProgressBar(SDL_Renderer* renderer,
                                        int x, int y, int w, int h, float progress) {
    SDL_SetRenderDrawColor(renderer, 30, 30, 50, 255);
    SDL_Rect bg = {x, y, w, h};
    SDL_RenderFillRect(renderer, &bg);
    int fillW = (int)(w * progress);
    if (fillW > 0) {
        SDL_SetRenderDrawColor(renderer, 50, 150, 255, 255);
        SDL_Rect fill = {x, y, fillW, h};
        SDL_RenderFillRect(renderer, &fill);
    }
    SDL_SetRenderDrawColor(renderer, 60, 60, 90, 255);
    SDL_RenderDrawRect(renderer, &bg);
}

void DownloadScreen::renderText(SDL_Renderer* renderer, const std::string& text,
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
