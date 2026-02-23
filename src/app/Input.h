#pragma once
#include <vpad/input.h>

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

    static Input read() {
        Input in;
        VPADStatus status;
        VPADReadError err;
        if (VPADRead(VPAD_CHAN_0, &status, 1, &err) > 0) {
            uint32_t btn = status.trigger;
            float lx = status.leftStick.x;
            float ly = status.leftStick.y;
            in.up    = (btn & VPAD_BUTTON_UP)    || ly >  0.5f;
            in.down  = (btn & VPAD_BUTTON_DOWN)  || ly < -0.5f;
            in.left  = (btn & VPAD_BUTTON_LEFT)  || lx < -0.5f;
            in.right = (btn & VPAD_BUTTON_RIGHT) || lx >  0.5f;
            in.a     = btn & VPAD_BUTTON_A;
            in.b     = btn & VPAD_BUTTON_B;
            in.l     = btn & VPAD_BUTTON_L;
            in.r     = btn & VPAD_BUTTON_R;
            in.y     = btn & VPAD_BUTTON_Y;
            in.x     = btn & VPAD_BUTTON_X;
            in.zl    = btn & VPAD_BUTTON_ZL;
            in.zr    = btn & VPAD_BUTTON_ZR;
            in.plus  = btn & VPAD_BUTTON_PLUS;
            in.minus = btn & VPAD_BUTTON_MINUS;
        }
        return in;
    }
};
