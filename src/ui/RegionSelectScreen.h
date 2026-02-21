#pragma once

#include "Screen.h"
#include "net/RepoManager.h"
#include "mods/InstallHelper.h"
#include <SDL2/SDL_ttf.h>
#include <string>
#include <vector>

class RegionSelectScreen : public Screen {
public:
    RegionSelectScreen(App* app, const Mod& mod,
                       const std::vector<TitleEntry>& entries);
    ~RegionSelectScreen() override;

    void onEnter() override;
    void onExit()  override;

    void handleInput(const Input& input) override;
    void update() override;
    void render(SDL_Renderer* renderer) override;

private:
    void renderText(SDL_Renderer* renderer, const std::string& text,
                    int x, int y, SDL_Color color, TTF_Font* font);

    Mod                      m_mod;
    std::vector<TitleEntry>  m_entries;
    int                      m_selected = 0;

    TTF_Font* m_fontLarge  = nullptr;
    TTF_Font* m_fontNormal = nullptr;
    TTF_Font* m_fontSmall  = nullptr;
};
