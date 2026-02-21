#pragma once
#include <SDL2/SDL.h>
#include "app/Input.h"

class App;

class Screen {
public:
    explicit Screen(App* app) : m_app(app) {}
    virtual ~Screen() = default;

    virtual void onEnter() {}
    virtual void onExit()  {}

    virtual void handleInput(const Input& input) = 0;
    virtual void update() = 0;
    virtual void render(SDL_Renderer* renderer) = 0;

protected:
    App* m_app;
};
