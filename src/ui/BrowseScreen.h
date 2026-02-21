#pragma once
#include "Screen.h"
#include <SDL2/SDL_ttf.h>
#include <string>

class BrowseScreen : public Screen {
public:
    explicit BrowseScreen(App* app);
    ~BrowseScreen() override;

    void onEnter() override;
    void handleEvent(const SDL_Event& event) override;
    void update() override;
    void render(SDL_Renderer* renderer) override;

private:
    void renderText(SDL_Renderer* renderer, const std::string& text,
                    int x, int y, SDL_Color color, TTF_Font* font);
    TTF_Font* m_font = nullptr;
};
