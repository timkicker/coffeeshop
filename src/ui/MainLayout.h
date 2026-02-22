#pragma once

#include "Screen.h"
#include "app/Config.h"
#include "net/RepoManager.h"
#include "mods/InstalledScanner.h"
#include "mods/ConflictChecker.h"
#include <SDL2/SDL_ttf.h>
#include <string>
#include <map>
#include <vector>
#include <thread>
#include <mutex>
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
    void renderSidebar(SDL_Renderer* renderer);
    void renderBrowse(SDL_Renderer* renderer);
    void renderInstalled(SDL_Renderer* renderer);
    void renderSettings(SDL_Renderer* renderer);
    void renderOnboarding(SDL_Renderer* renderer);
    void renderText(SDL_Renderer* renderer, const std::string& text,
                    int x, int y, SDL_Color color, TTF_Font* font);

    // Browse
    void handleBrowseInput(const Input& input);

    // Installed
    void handleInstalledInput(const Input& input);

    // Settings
    void handleSettingsInput(const Input& input);
    void refreshInstalled();

    TTF_Font* m_fontNormal = nullptr;
    TTF_Font* m_fontSmall  = nullptr;
    TTF_Font* m_fontTiny   = nullptr;

    enum class Tab { Browse, Installed, Settings };
    Tab m_activeTab = Tab::Browse;

    Config m_config;
    bool   m_showOnboarding = false;

    // Browse state
    enum class FetchState { Idle, Loading, Done, Error };
    std::atomic<FetchState> m_fetchState { FetchState::Idle };
    std::string             m_fetchError;
    std::map<std::string, std::string> m_repoStatus; // url -> "OK" or error msg
    std::thread             m_fetchThread;
    std::mutex              m_repoMutex;
    Repo                    m_repo;
    int m_selectedGame = 0;
    int m_selectedMod  = 0;

    enum class SortMode { Default, NameAZ, Version };
    SortMode m_sortMode = SortMode::Default;

    // Installed state
    std::vector<InstalledMod> m_installedMods;
    int                       m_selectedInstalled = 0;
    bool                      m_installedDirty    = true;
    bool                      m_confirmUninstall  = false;
    bool                      m_showConflict      = false;
    ConflictResult            m_conflictResult;
    bool                      m_showStartupConflicts = false;
    struct StartupConflict { std::string modName; std::vector<std::string> conflicts; };
    std::vector<StartupConflict> m_startupConflicts;

    // Settings state
    int                       m_settingsSelected  = 0;
    int                       m_settingsScroll    = 0;
    bool                      m_showLog           = false;
    int                       m_logScroll         = 0;

    static constexpr int SIDEBAR_W = 220;
};
