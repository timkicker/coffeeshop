#pragma once
#include <string>
#include <unordered_map>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

// Caches SDL_Texture* keyed on (font ptr, color, text). Eliminates the
// per-frame TTF_RenderUTF8_Blended + SDL_CreateTextureFromSurface cost that
// CLAUDE.md explicitly forbids.
//
// Usage in a Screen::render():
//     auto* tex = TextCache::get().texture(renderer, font, color, "Hello");
//     int w, h; TextCache::get().sizeOf(tex, &w, &h);
//     SDL_Rect dst = {x, y, w, h};
//     SDL_RenderCopy(renderer, tex, nullptr, &dst);
//
// Lifecycle:
//   - call prune(currentFrame) once per N frames to evict unused textures
//   - call clear() before SDL_DestroyRenderer (App destructor)
class TextCache {
public:
    static TextCache& get();

    // Returns a cached or freshly-rendered texture. Returns nullptr if any
    // SDL/TTF call fails. Texture is owned by TextCache; caller must NOT
    // free it.
    SDL_Texture* texture(SDL_Renderer* renderer, TTF_Font* font,
                         SDL_Color color, const std::string& text);

    // Lookup the rendered size for a previously-fetched texture.
    void sizeOf(SDL_Texture* tex, int* w, int* h);

    // Mark current frame for LRU bookkeeping. Call from App::run once per frame.
    void tick();

    // Evict textures unused for `framesUnused` ticks. Call once per second
    // or so to bound memory.
    void prune(int framesUnused = 600);

    // Free all textures. MUST be called before SDL_DestroyRenderer.
    void clear();

private:
    TextCache() = default;
    ~TextCache() { clear(); }
    TextCache(const TextCache&) = delete;
    TextCache& operator=(const TextCache&) = delete;

    struct Key {
        TTF_Font*   font;
        uint32_t    color;       // packed R/G/B/A
        std::string text;
        bool operator==(const Key& o) const {
            return font == o.font && color == o.color && text == o.text;
        }
    };
    struct KeyHash {
        size_t operator()(const Key& k) const {
            size_t h = std::hash<void*>{}((void*)k.font);
            h ^= std::hash<uint32_t>{}(k.color) + 0x9e3779b9 + (h << 6) + (h >> 2);
            h ^= std::hash<std::string>{}(k.text) + 0x9e3779b9 + (h << 6) + (h >> 2);
            return h;
        }
    };
    struct Entry {
        SDL_Texture* tex      = nullptr;
        int          w        = 0;
        int          h        = 0;
        int          lastUsed = 0;
    };

    std::unordered_map<Key, Entry, KeyHash> m_entries;
    int m_frame = 0;

    static uint32_t pack(SDL_Color c) {
        return (uint32_t(c.r) << 24) | (uint32_t(c.g) << 16)
             | (uint32_t(c.b) << 8)  | uint32_t(c.a);
    }
};
