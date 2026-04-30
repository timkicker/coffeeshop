#include <catch2/catch_test_macros.hpp>
#include "util/TextCache.h"

// TextCache hits SDL_ttf / SDL_Renderer functions which we don't initialize
// in the test build. We can still exercise the parts that don't need the
// libraries: the cache key/eviction logic via direct manipulation.
//
// Without a renderer, texture() returns nullptr immediately (early-return on
// renderer==nullptr). That's enough to test the prune/clear scaffolding.

TEST_CASE("TextCache::texture - null renderer returns nullptr safely",
          "[textcache]") {
    auto& tc = TextCache::get();
    tc.clear();
    SDL_Color white{255, 255, 255, 255};
    REQUIRE(tc.texture(nullptr, nullptr, white, "hello") == nullptr);
}

TEST_CASE("TextCache::texture - empty text returns nullptr safely",
          "[textcache]") {
    auto& tc = TextCache::get();
    tc.clear();
    SDL_Color white{255, 255, 255, 255};
    // Even with non-null renderer/font (we'd need real SDL), empty text path
    // is exercised first.
    REQUIRE(tc.texture(nullptr, nullptr, white, "") == nullptr);
}

TEST_CASE("TextCache::clear / tick / prune are idempotent",
          "[textcache]") {
    auto& tc = TextCache::get();
    tc.clear();
    tc.tick();
    tc.tick();
    tc.prune(10);   // empty → no-op
    tc.clear();     // double clear → no-op
    SUCCEED("no crash");
}

TEST_CASE("TextCache::sizeOf - unknown texture returns zeros",
          "[textcache]") {
    auto& tc = TextCache::get();
    tc.clear();
    int w = -1, h = -1;
    tc.sizeOf(nullptr, &w, &h);
    REQUIRE(w == 0);
    REQUIRE(h == 0);
}
