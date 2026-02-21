#pragma once

#include "Screen.h"
#include "net/DownloadQueue.h"
#include <SDL2/SDL_ttf.h>

class DownloadQueueScreen : public Screen {
public:
    explicit DownloadQueueScreen(App* app);
    ~DownloadQueueScreen() override;

    void onEnter() override;
    void onExit()  override;

    void handleInput(const Input& input) override;
    void update() override;
    void render(SDL_Renderer* renderer) override;

private:
    void renderText(SDL_Renderer* renderer, const std::string& text,
                    int x, int y, SDL_Color color, TTF_Font* font);
    void renderProgressBar(SDL_Renderer* renderer,
                           int x, int y, int w, int h, float progress);

    int       m_selectedIdx = 0;
    TTF_Font* m_fontLarge  = nullptr;
    TTF_Font* m_fontNormal = nullptr;
    TTF_Font* m_fontSmall  = nullptr;
    TTF_Font* m_fontTiny   = nullptr;
};
