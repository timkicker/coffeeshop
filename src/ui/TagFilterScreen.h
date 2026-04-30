#pragma once
#include "Screen.h"
#include <SDL2/SDL_ttf.h>
#include <set>
#include <string>
#include <vector>
#include <functional>

// Modal overlay that shows the available tags from the current repo and lets
// the user toggle which ones the browse tab filters on. Result is written
// back via a callback when the user dismisses (B button).
class TagFilterScreen : public Screen {
public:
    using Callback = std::function<void(std::set<std::string>)>;
    TagFilterScreen(App* app,
                    std::vector<std::string> available,
                    std::set<std::string> selected,
                    Callback onClose);

    void onEnter() override;
    void onExit() override;
    void handleInput(const Input& input) override;
    void update() override;
    void render(SDL_Renderer* renderer) override;

private:
    void renderText(SDL_Renderer* r, const std::string& text,
                    int x, int y, SDL_Color color, TTF_Font* font);

    std::vector<std::string> m_tags;
    std::set<std::string>    m_selected;
    Callback                 m_onClose;
    int                      m_cursor = 0;
    int                      m_scroll = 0;

    TTF_Font* m_fontLarge  = nullptr;
    TTF_Font* m_fontNormal = nullptr;
    TTF_Font* m_fontSmall  = nullptr;
};
