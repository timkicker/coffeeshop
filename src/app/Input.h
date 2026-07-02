#pragma once
#include <cstdint>

// Game input, populated from SDL joystick events. The same code path works
// for every Wii U input device (GamePad, Pro Controller, Wii Remote +/-
// extensions, Classic Controller) because SDL2-wuhb's WIIU_JoystickInit
// already initialises VPAD + KPAD + URCC and emits a single SDL_JOYBUTTONDOWN
// per physical button press, with a canonical button index that matches
// across all device types. See devkitPro/SDL src/joystick/wiiu for the
// per-device index maps:
//
//   0=A           1=B           2=X            3=Y
//   4=STICK_L_PUSH  5=STICK_R_PUSH
//   6=L           7=R           8=ZL           9=ZR
//   10=PLUS       11=MINUS
//   12=LEFT       13=UP         14=RIGHT       15=DOWN
//   16=LSTICK_LEFT  17=LSTICK_UP  18=LSTICK_RIGHT  19=LSTICK_DOWN
//   20=RSTICK_LEFT  21=RSTICK_UP  22=RSTICK_RIGHT  23=RSTICK_DOWN
//
// SDL handles the stick-as-d-pad emulation for us: when the stick crosses
// the threshold, SDL fires a SDL_JOYBUTTONDOWN with the corresponding
// LSTICK_* index, then SDL_JOYBUTTONUP when it returns to centre. Same
// edge-triggered semantics as the actual D-pad. We don't need to poll axes.
//
// App::run's event loop calls Input::onJoyButtonDown() for each
// SDL_JOYBUTTONDOWN. Per frame, App::update() calls Input::read() which
// returns a freshly drained snapshot.
struct Input {
    bool up    = false;
    bool down  = false;
    bool left  = false;
    bool right = false;
    bool a     = false;
    bool b     = false;
    bool l     = false;
    bool r     = false;
    bool y     = false;
    bool x     = false;
    bool zl    = false;
    bool zr    = false;
    bool plus  = false;
    bool minus = false;
    bool home  = false; // never fires: Aroma intercepts HOME before us

    // SDL2-wuhb button indices. Order MUST match the arrays in the SDL
    // joystick driver (devkitPro/SDL src/joystick/wiiu/SDL_wiiujoystick.h).
    enum BtnIdx {
        BTN_A = 0, BTN_B, BTN_X, BTN_Y,
        BTN_STICK_L_PUSH, BTN_STICK_R_PUSH,
        BTN_L, BTN_R, BTN_ZL, BTN_ZR,
        BTN_PLUS, BTN_MINUS,
        BTN_LEFT, BTN_UP, BTN_RIGHT, BTN_DOWN,
        BTN_LSTICK_LEFT, BTN_LSTICK_UP, BTN_LSTICK_RIGHT, BTN_LSTICK_DOWN,
        BTN_RSTICK_LEFT, BTN_RSTICK_UP, BTN_RSTICK_RIGHT, BTN_RSTICK_DOWN,
        BTN_COUNT
    };

    // Called from App::run's event loop when a SDL_JOYBUTTONDOWN arrives.
    // Accumulates presses across the frame; read() drains.
    static void onJoyButtonDown(int buttonIdx);

    // Drain the accumulator into a fresh Input. Called once per frame in
    // App::update(), BEFORE the active screen's handleInput. Resets the
    // accumulator so the same press isn't seen twice.
    static Input read();

private:
    // Bitfield of indices pressed since last read(). 24 bits used; uint32_t
    // is comfortable. Not atomic: SDL_PollEvent runs on the main thread, as
    // does read().
    static uint32_t s_pressedBits;
};
