#pragma once

#include "Screen.h"
#include "net/DownloadManager.h"
#include "net/RepoManager.h"
#include <SDL2/SDL_ttf.h>
#include <string>
#include <thread>

class DownloadScreen : public Screen {
public:
    DownloadScreen(App* app, const Mod& mod, const std::string& titleId);
    ~DownloadScreen() override;

    void onEnter() override;
    void onExit()  override;

    void handleInput(const Input& input) override;
    void update() override;
    void render(SDL_Renderer* renderer) override;

private:
    void renderText(SDL_Renderer* renderer, const std::string& text,
                    int x, int y, SDL_Color color, TTF_Font* font);
    void renderProgressBar(SDL_Renderer* renderer, int x, int y, int w, int h, float progress);

    Mod         m_mod;
    std::string m_titleId;

    DownloadManager m_dm;
    std::thread     m_thread;

    TTF_Font* m_fontLarge  = nullptr;
    TTF_Font* m_fontNormal = nullptr;
    TTF_Font* m_fontSmall  = nullptr;
};
