#include "DownloadQueueScreen.h"
#include "app/App.h"

static constexpr const char* FONT_PATH = "/vol/content/Roboto-Regular.ttf";

DownloadQueueScreen::DownloadQueueScreen(App* app) : Screen(app) {}

DownloadQueueScreen::~DownloadQueueScreen() {
    if (m_fontLarge)  TTF_CloseFont(m_fontLarge);
    if (m_fontNormal) TTF_CloseFont(m_fontNormal);
    if (m_fontSmall)  TTF_CloseFont(m_fontSmall);
    if (m_fontTiny)   TTF_CloseFont(m_fontTiny);
}

void DownloadQueueScreen::onEnter() {
    m_fontLarge  = TTF_OpenFont(FONT_PATH, 32);
    m_fontNormal = TTF_OpenFont(FONT_PATH, 22);
    m_fontSmall  = TTF_OpenFont(FONT_PATH, 17);
    m_fontTiny   = TTF_OpenFont(FONT_PATH, 13);
}

void DownloadQueueScreen::onExit() {}

void DownloadQueueScreen::handleInput(const Input& input) {
    auto jobs = DownloadQueue::get().jobs();

    if (input.b) {
        // If selected job is an error, dismiss it; otherwise go back
        if (!jobs.empty() && m_selectedIdx < (int)jobs.size() &&
            jobs[m_selectedIdx].state == DownloadJob::State::Error) {
            DownloadQueue::get().dismissError(m_selectedIdx);
        } else {
            m_app->popScreen();
        }
        return;
    }

    if (input.up)   m_selectedIdx = std::max(0, m_selectedIdx - 1);
    if (input.down) m_selectedIdx = std::min((int)jobs.size() - 1, m_selectedIdx + 1);
}

void DownloadQueueScreen::update() {}

void DownloadQueueScreen::render(SDL_Renderer* renderer) {
    const int W = m_app->screenWidth();
    const int H = m_app->screenHeight();

    SDL_Color white  = {255, 255, 255, 255};
    SDL_Color grey   = {130, 130, 155, 255};
    SDL_Color accent = { 80, 180, 255, 255};
    SDL_Color green  = { 55, 190,  95, 255};
    SDL_Color red    = {220,  70,  70, 255};
    SDL_Color yellow = {255, 200,  50, 255};

    // Background
    SDL_SetRenderDrawColor(renderer, 12, 12, 20, 255);
    SDL_Rect bg = {0, 0, W, H};
    SDL_RenderFillRect(renderer, &bg);

    // Top bar
    SDL_SetRenderDrawColor(renderer, 20, 20, 33, 255);
    SDL_Rect topbar = {0, 0, W, 50};
    SDL_RenderFillRect(renderer, &topbar);
    SDL_SetRenderDrawColor(renderer, 45, 45, 65, 255);
    SDL_RenderDrawLine(renderer, 0, 50, W, 50);

    if (m_fontSmall) renderText(renderer, "B  Back", 16, 14, accent, m_fontSmall);
    if (m_fontLarge) renderText(renderer, "Downloads", 110, 9, white, m_fontLarge);

    auto jobs = DownloadQueue::get().jobs();

    if (jobs.empty()) {
        if (m_fontNormal)
            renderText(renderer, "No downloads.", W/2 - 80, H/2 - 14, grey, m_fontNormal);
        return;
    }

    int y = 70;
    int rowIdx = 0;
    for (auto& job : jobs) {
        bool isActive = (job.state == DownloadJob::State::Downloading ||
                        job.state == DownloadJob::State::Extracting);

        bool isSelected = (rowIdx == m_selectedIdx);
        // Row background
        SDL_SetRenderDrawColor(renderer, isActive ? 25 : 18, isActive ? 25 : 18, isActive ? 42 : 30, 255);
        SDL_Rect row = {20, y, W - 40, isActive ? 80 : 50};
        SDL_RenderFillRect(renderer, &row);
        SDL_Color borderCol = isSelected ? SDL_Color{80,180,255,255} : SDL_Color{40,40,60,255};
        SDL_SetRenderDrawColor(renderer, borderCol.r, borderCol.g, borderCol.b, 255);
        SDL_RenderDrawRect(renderer, &row);

        // Status color + label
        SDL_Color stateColor;
        std::string stateLabel;
        switch (job.state) {
            case DownloadJob::State::Pending:
                stateColor = grey;   stateLabel = "Pending";     break;
            case DownloadJob::State::Downloading:
                stateColor = accent; stateLabel = "Downloading"; break;
            case DownloadJob::State::Extracting:
                stateColor = yellow; stateLabel = "Installing";  break;
            case DownloadJob::State::Done:
                stateColor = green;  stateLabel = "Done";        break;
            case DownloadJob::State::Error:
                stateColor = red;    stateLabel = "Error";       break;
        }

        if (m_fontNormal)
            renderText(renderer, job.mod.name, 36, y + 8, white, m_fontNormal);
        if (m_fontTiny)
            renderText(renderer, stateLabel, W - 120, y + 12, stateColor, m_fontTiny);

        if (isActive) {
            renderProgressBar(renderer, 36, y + 40, W - 72, 18, job.progress);
        } else if (job.state == DownloadJob::State::Error && m_fontTiny) {
            renderText(renderer, job.error, 36, y + 32, red, m_fontTiny);
        }

        y += (isActive ? 80 : 50) + 8;
        rowIdx++;
        if (y > H - 60) break;
    }

    // Bottom hint
    if (m_fontSmall) {
        bool errorSelected = !jobs.empty() && m_selectedIdx < (int)jobs.size() &&
                             jobs[m_selectedIdx].state == DownloadJob::State::Error;
        std::string hint = errorSelected ? "B: Dismiss error   Up/Down: navigate" : "B: Back   Up/Down: navigate";
        renderText(renderer, hint, W/2 - 160, H - 35, grey, m_fontSmall);
    }
}

void DownloadQueueScreen::renderProgressBar(SDL_Renderer* renderer,
                                             int x, int y, int w, int h, float progress) {
    SDL_SetRenderDrawColor(renderer, 30, 30, 50, 255);
    SDL_Rect bg = {x, y, w, h};
    SDL_RenderFillRect(renderer, &bg);
    int fw = (int)(w * progress);
    if (fw > 0) {
        SDL_SetRenderDrawColor(renderer, 50, 150, 255, 255);
        SDL_Rect fill = {x, y, fw, h};
        SDL_RenderFillRect(renderer, &fill);
    }
    SDL_SetRenderDrawColor(renderer, 60, 60, 90, 255);
    SDL_RenderDrawRect(renderer, &bg);
}

void DownloadQueueScreen::renderText(SDL_Renderer* renderer, const std::string& text,
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
