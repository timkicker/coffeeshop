#include "TextCache.h"
#include "util/Logger.h"

TextCache& TextCache::get() {
    static TextCache instance;
    return instance;
}

SDL_Texture* TextCache::texture(SDL_Renderer* renderer, TTF_Font* font,
                                 SDL_Color color, const std::string& text) {
    if (!renderer || !font || text.empty()) return nullptr;

    Key key{font, pack(color), text};
    auto it = m_entries.find(key);
    if (it != m_entries.end()) {
        it->second.lastUsed = m_frame;
        return it->second.tex;
    }

    SDL_Surface* surf = TTF_RenderUTF8_Blended(font, text.c_str(), color);
    if (!surf) return nullptr;
    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
    int w = surf->w, h = surf->h;
    SDL_FreeSurface(surf);
    if (!tex) return nullptr;

    Entry e{tex, w, h, m_frame};
    m_entries.emplace(key, e);
    return tex;
}

void TextCache::sizeOf(SDL_Texture* tex, int* w, int* h) {
    if (!tex) { if (w) *w = 0; if (h) *h = 0; return; }
    // Linear scan is fine -- typical cache size is well under 200.
    for (auto& [_k, e] : m_entries) {
        if (e.tex == tex) {
            if (w) *w = e.w;
            if (h) *h = e.h;
            return;
        }
    }
    // Fall back to SDL query for textures we somehow don't track.
    if (w || h) {
        int qw = 0, qh = 0;
        SDL_QueryTexture(tex, nullptr, nullptr, &qw, &qh);
        if (w) *w = qw;
        if (h) *h = qh;
    }
}

void TextCache::tick() {
    m_frame++;
}

void TextCache::prune(int framesUnused) {
    int cutoff = m_frame - framesUnused;
    for (auto it = m_entries.begin(); it != m_entries.end();) {
        if (it->second.lastUsed < cutoff) {
            SDL_DestroyTexture(it->second.tex);
            it = m_entries.erase(it);
        } else {
            ++it;
        }
    }
}

void TextCache::clear() {
    for (auto& [_k, e] : m_entries) {
        if (e.tex) SDL_DestroyTexture(e.tex);
    }
    m_entries.clear();
}
