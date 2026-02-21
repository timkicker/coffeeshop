#pragma once

#include "Screen.h"
#include <SDL2/SDL_ttf.h>
#include <string>
#include <vector>

class MainMenuScreen : public Screen {
public:
    explicit MainMenuScreen(App* app);
    ~MainMenuScreen() override;

    void onEnter() override;
    void onExit()  override;

    void handleEvent(const SDL_Event& event) override;
    void update() override;
    void render(SDL_Renderer* renderer) override;

private:
    void handleSelect();
    void renderText(SDL_Renderer* renderer, const std::string& text,
                    int x, int y, SDL_Color color, TTF_Font* font);

    TTF_Font* m_fontLarge = nullptr;
    TTF_Font* m_fontSmall = nullptr;

    struct MenuItem { std::string label; };
    std::vector<MenuItem> m_items;
    int m_selectedIndex = 0;
};
