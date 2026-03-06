#pragma once
#include "Screen.h"
#include "app/Config.h"
#include <SDL2/SDL_ttf.h>
#include <string>
#include <vector>

class SettingsScreen : public Screen {
public:
    SettingsScreen(App* app, const Config& config);
    ~SettingsScreen() override;

    void onEnter() override;
    void onExit()  override;
    void handleInput(const Input& input) override;
    void update() override;
    void render(SDL_Renderer* renderer) override;

private:
    struct Item {
        std::string  label;
        std::string  value;
        bool         isButton = false;
        bool         isHeader = false;
        SDL_Texture* labelTex = nullptr;
        SDL_Rect     labelRect{};
        SDL_Texture* valueTex = nullptr;
        SDL_Rect     valueRect{};
    };

    void buildItems();
    void buildTextureCache(SDL_Renderer* renderer);
    void freeItemTextures();
    void renderText(SDL_Renderer* renderer, const std::string& text,
                    int x, int y, SDL_Color color, TTF_Font* font);

    Config               m_config;
    std::vector<Item>    m_items;
    int                  m_selectedIdx  = 0;
    int                  m_scrollOffset = 0;
    bool                 m_textureCacheDirty = true;

    // Log viewer state
    bool                 m_showLog      = false;
    int                  m_logScroll    = 0;

    TTF_Font* m_fontLarge  = nullptr;
    TTF_Font* m_fontNormal = nullptr;
    TTF_Font* m_fontSmall  = nullptr;
    TTF_Font* m_fontTiny   = nullptr;
};
