#include <catch2/catch_test_macros.hpp>
#include "app/Input.h"

// Tests the SDL-event-driven Input accumulator. The fields we map:
//   A/B/X/Y, L/R/ZL/ZR, +/-,  D-pad (which also gets L-stick emulation OR'd in)
// Out of scope here: actual SDL events. We poke onJoyButtonDown directly with
// the same button indices SDL2-wuhb emits.

TEST_CASE("Input: empty by default", "[input]") {
    // drain whatever might be there from a previous test
    Input::read();
    Input in = Input::read();
    REQUIRE_FALSE(in.a);
    REQUIRE_FALSE(in.b);
    REQUIRE_FALSE(in.up);
    REQUIRE_FALSE(in.minus);
}

TEST_CASE("Input: single button press is reported once then drained", "[input]") {
    Input::read(); // drain
    Input::onJoyButtonDown(Input::BTN_A);

    Input in1 = Input::read();
    REQUIRE(in1.a);
    REQUIRE_FALSE(in1.b);

    // A second read with no further presses must report nothing.
    Input in2 = Input::read();
    REQUIRE_FALSE(in2.a);
}

TEST_CASE("Input: multiple presses in the same frame combine", "[input]") {
    Input::read();
    Input::onJoyButtonDown(Input::BTN_A);
    Input::onJoyButtonDown(Input::BTN_B);
    Input::onJoyButtonDown(Input::BTN_PLUS);
    Input in = Input::read();
    REQUIRE(in.a);
    REQUIRE(in.b);
    REQUIRE(in.plus);
    REQUIRE_FALSE(in.minus);
}

TEST_CASE("Input: each button index maps to expected field", "[input]") {
    struct Case { int idx; bool Input::* field; };
    Case cases[] = {
        {Input::BTN_A,     &Input::a},
        {Input::BTN_B,     &Input::b},
        {Input::BTN_X,     &Input::x},
        {Input::BTN_Y,     &Input::y},
        {Input::BTN_L,     &Input::l},
        {Input::BTN_R,     &Input::r},
        {Input::BTN_ZL,    &Input::zl},
        {Input::BTN_ZR,    &Input::zr},
        {Input::BTN_PLUS,  &Input::plus},
        {Input::BTN_MINUS, &Input::minus},
        {Input::BTN_UP,    &Input::up},
        {Input::BTN_DOWN,  &Input::down},
        {Input::BTN_LEFT,  &Input::left},
        {Input::BTN_RIGHT, &Input::right},
    };
    for (const auto& c : cases) {
        Input::read(); // drain
        Input::onJoyButtonDown(c.idx);
        Input in = Input::read();
        REQUIRE(in.*c.field);
    }
}

TEST_CASE("Input: L-stick emulation OR'd into D-pad", "[input]") {
    Input::read();
    Input::onJoyButtonDown(Input::BTN_LSTICK_UP);
    REQUIRE(Input::read().up);

    Input::onJoyButtonDown(Input::BTN_LSTICK_DOWN);
    REQUIRE(Input::read().down);

    Input::onJoyButtonDown(Input::BTN_LSTICK_LEFT);
    REQUIRE(Input::read().left);

    Input::onJoyButtonDown(Input::BTN_LSTICK_RIGHT);
    REQUIRE(Input::read().right);
}

TEST_CASE("Input: R-stick is intentionally ignored for navigation", "[input]") {
    Input::read();
    Input::onJoyButtonDown(Input::BTN_RSTICK_UP);
    Input::onJoyButtonDown(Input::BTN_RSTICK_DOWN);
    Input::onJoyButtonDown(Input::BTN_RSTICK_LEFT);
    Input::onJoyButtonDown(Input::BTN_RSTICK_RIGHT);
    Input in = Input::read();
    REQUIRE_FALSE(in.up);
    REQUIRE_FALSE(in.down);
    REQUIRE_FALSE(in.left);
    REQUIRE_FALSE(in.right);
}

TEST_CASE("Input: out-of-range indices silently ignored", "[input]") {
    Input::read();
    Input::onJoyButtonDown(-1);
    Input::onJoyButtonDown(99);
    Input::onJoyButtonDown(Input::BTN_COUNT);
    Input in = Input::read();
    REQUIRE_FALSE(in.a);
    REQUIRE_FALSE(in.b);
}

TEST_CASE("Input: home is never reported (Aroma intercepts)", "[input]") {
    Input::read();
    // No public way to set it -- field defaults false and read() never
    // sets it. Just confirm.
    Input in = Input::read();
    REQUIRE_FALSE(in.home);
}
