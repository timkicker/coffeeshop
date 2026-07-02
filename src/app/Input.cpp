#include "Input.h"

uint32_t Input::s_pressedBits = 0;

void Input::onJoyButtonDown(int buttonIdx) {
    if (buttonIdx < 0 || buttonIdx >= BTN_COUNT) return;
    s_pressedBits |= (1u << buttonIdx);
}

Input Input::read() {
    Input in;
    const uint32_t bits = s_pressedBits;
    s_pressedBits = 0;

    auto has = [bits](BtnIdx b) { return (bits & (1u << b)) != 0; };

    in.a     = has(BTN_A);
    in.b     = has(BTN_B);
    in.x     = has(BTN_X);
    in.y     = has(BTN_Y);
    in.l     = has(BTN_L);
    in.r     = has(BTN_R);
    in.zl    = has(BTN_ZL);
    in.zr    = has(BTN_ZR);
    in.plus  = has(BTN_PLUS);
    in.minus = has(BTN_MINUS);
    // D-pad OR'd with left-stick-emulation so sticks navigate the UI just
    // like the D-pad. Right stick is intentionally ignored: nothing in the
    // app uses it, and an over-eager grip would otherwise trigger nav events.
    in.up    = has(BTN_UP)    || has(BTN_LSTICK_UP);
    in.down  = has(BTN_DOWN)  || has(BTN_LSTICK_DOWN);
    in.left  = has(BTN_LEFT)  || has(BTN_LSTICK_LEFT);
    in.right = has(BTN_RIGHT) || has(BTN_LSTICK_RIGHT);
    // home stays false; Aroma intercepts HOME before SDL sees it.

    return in;
}
