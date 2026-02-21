#pragma once

#include "Screen.h"
#include "app/Config.h"
#include "net/RepoManager.h"
#include <SDL2/SDL_ttf.h>
#include <string>
#include <thread>
#include <atomic>

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
    void renderBrowse(SDL_Renderer* renderer, int x, int w, int h);
    void renderText(SDL_Renderer* renderer, const std::string& text,
                    int x, int y, SDL_Color color, TTF_Font* font);

    TTF_Font* m_fontNormal = nullptr;
    TTF_Font* m_fontSmall  = nullptr;

    enum class Tab { Browse, Installed, Settings };
    Tab m_activeTab = Tab::Browse;

    Config      m_config;
    RepoManager m_repo;
    bool        m_showOnboarding = false;

    // Async fetch state
    enum class FetchState { Idle, Loading, Done, Error };
    std::atomic<FetchState> m_fetchState { FetchState::Idle };
    std::thread             m_fetchThread;
    std::string             m_fetchError;

    // Browse state
    int m_selectedGame = 0;
    int m_selectedMod  = 0;

    static constexpr int SIDEBAR_W = 220;
};
