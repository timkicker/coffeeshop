#pragma once

#include "Screen.h"
#include "net/RepoManager.h"
#include "mods/InstallChecker.h"
#include <SDL2/SDL_ttf.h>
#include <string>
#include <vector>

class DetailScreen : public Screen {
public:
    DetailScreen(App* app, const Mod& mod, const std::string& gameName,
                 const std::vector<std::string>& titleIds);
    ~DetailScreen() override;

    void onEnter() override;
    void onExit()  override;

    void handleInput(const Input& input) override;
    void update() override;
    void render(SDL_Renderer* renderer) override;

private:
    void renderText(SDL_Renderer* renderer, const std::string& text,
                    int x, int y, SDL_Color color, TTF_Font* font);
    void renderWrappedText(SDL_Renderer* renderer, const std::string& text,
                           int x, int y, int maxW, SDL_Color color, TTF_Font* font);

    Mod                      m_mod;
    std::string              m_gameName;
    std::vector<std::string> m_titleIds;
    int                      m_screenshotIndex = 0;
    InstallStatus            m_installStatus;

    TTF_Font* m_fontLarge  = nullptr;
    TTF_Font* m_fontNormal = nullptr;
    TTF_Font* m_fontSmall  = nullptr;
    TTF_Font* m_fontTiny   = nullptr;
};
