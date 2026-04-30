#include "app/Paths.h"

// Paths::sdMounted is a static member - needs a definition in exactly one TU
bool Paths::sdMounted = false;
std::string Paths::testRootOverride;

// curl stubs are provided by curl_mock.cpp (real-ish behavior driven by tests)

// SDL/TTF stubs -- TextCache.cpp pulls these symbols in but the tests only
// exercise null-renderer/empty-text paths that bail before any real SDL work.
extern "C" {
    void SDL_DestroyTexture(void*) {}
    void SDL_FreeSurface(void*)    {}
    int  SDL_QueryTexture(void*, unsigned int*, int*, int* w, int* h) {
        if (w) *w = 0; if (h) *h = 0; return 0;
    }
    void* SDL_CreateTextureFromSurface(void*, void*) { return nullptr; }
    void* TTF_RenderUTF8_Blended(void*, const char*, ...) { return nullptr; }
}
