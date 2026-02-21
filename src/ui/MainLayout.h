#pragma once

#include "Screen.h"
#include "app/Config.h"
#include <SDL2/SDL_ttf.h>
#include <string>

class MainLayout : public Screen {
public:
    explicit MainLayout(App* app);
    ~MainLayout() override;

    void onEnter() override;
    void onExit()  override;

    void handleInput(const Input& input) override;
    void update() override;
    void render(SDL_Renderer* renderer) override;

private:
    void renderOnboarding(SDL_Renderer* renderer);
    void renderSidebar(SDL_Renderer* renderer);
    void renderContent(SDL_Renderer* renderer);
    void renderText(SDL_Renderer* renderer, const std::string& text,
                    int x, int y, SDL_Color color, TTF_Font* font);

    TTF_Font* m_fontNormal = nullptr;
    TTF_Font* m_fontSmall  = nullptr;

    enum class Tab { Browse, Installed, Settings };
    Tab m_activeTab = Tab::Browse;

    Config m_config;
    bool   m_showOnboarding = false;

    static constexpr int SIDEBAR_W = 220;
};
