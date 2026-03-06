#pragma once

#include <SDL2/SDL.h>
#include <memory>
#include <vector>

class Screen;

class App {
public:
    App();
    ~App();

    App(const App&) = delete;
    App& operator=(const App&) = delete;

    bool init();
    void run();
    void quit();
    void startExit(); // SYSLaunchMenu + drain loop
    bool isExiting() const { return m_exiting; }

    SDL_Renderer* renderer() const { return m_renderer; }
    SDL_Window*   window()   const { return m_window; }
    int screenWidth()        const { return m_screenW; }
    int screenHeight()       const { return m_screenH; }

    void pushScreen(std::unique_ptr<Screen> screen);
    void popScreen();

private:
    void update();
    void render();

    SDL_Window*   m_window   = nullptr;
    SDL_Renderer* m_renderer = nullptr;

    int  m_screenW = 1280;
    int  m_screenH = 720;
    bool m_running = false;
    bool m_exiting = false;

    std::vector<std::unique_ptr<Screen>> m_screens;
};
