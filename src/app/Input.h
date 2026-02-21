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

    static Input read() {
        Input in;
        VPADStatus status;
        VPADReadError err;
        if (VPADRead(VPAD_CHAN_0, &status, 1, &err) > 0) {
            uint32_t btn = status.trigger;
            in.up    = btn & VPAD_BUTTON_UP;
            in.down  = btn & VPAD_BUTTON_DOWN;
            in.left  = btn & VPAD_BUTTON_LEFT;
            in.right = btn & VPAD_BUTTON_RIGHT;
            in.a     = btn & VPAD_BUTTON_A;
            in.b     = btn & VPAD_BUTTON_B;
            in.l     = btn & VPAD_BUTTON_L;
            in.r     = btn & VPAD_BUTTON_R;
        }
        return in;
    }
};
