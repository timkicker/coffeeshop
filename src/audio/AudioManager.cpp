#include "AudioManager.h"
#include "util/Logger.h"
#include <SDL2/SDL.h>

AudioManager& AudioManager::get() {
    static AudioManager instance;
    return instance;
}

static std::string sndPath(const char* f) {
    return std::string("/vol/content/sounds/") + f;
}

void AudioManager::init() {
    if (m_initialized) return;
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
        LOG_WARN("AudioManager: Mix_OpenAudio failed: %s", Mix_GetError());
        return;
    }
    Mix_Init(MIX_INIT_OGG | MIX_INIT_MP3);

    struct { SoundId id; const char* file; } sounds[] = {
        { SoundId::Navigate,         "navigate_all.wav"              },
        { SoundId::DownloadStart,    "download_button_hit.wav"       },
        { SoundId::DownloadFinished, "download_finished.wav"         },
        { SoundId::Error,            "error.wav"                     },
        { SoundId::ModActivated,     "mod_activated.wav"             },
        { SoundId::ModDeactivated,   "mod_deactivated.wav"           },
        { SoundId::Startup,          "startup.wav"                   },
        { SoundId::ConflictWarning,  "warning_conflict_detected.wav" },
    };
    for (auto& s : sounds) {
        Mix_Chunk* c = Mix_LoadWAV(sndPath(s.file).c_str());
        if (c) m_sounds[(int)s.id] = c;
        else LOG_WARN("AudioManager: failed to load %s: %s", s.file, Mix_GetError());
    }
    m_initialized = true;
    LOG_INFO("AudioManager: initialized (%zu sounds loaded)", m_sounds.size());
}

void AudioManager::shutdown() {
    if (!m_initialized) return;
    stopMusic();
    for (auto& [id, chunk] : m_sounds) Mix_FreeChunk(chunk);
    m_sounds.clear();
    if (m_music) { Mix_FreeMusic(m_music); m_music = nullptr; }
    Mix_CloseAudio();
    Mix_Quit();
    m_initialized = false;
}

void AudioManager::playSound(SoundId id) {
    if (!m_initialized || !m_soundEnabled) return;
    auto it = m_sounds.find((int)id);
    if (it != m_sounds.end()) Mix_PlayChannel(-1, it->second, 0);
}

void AudioManager::playMusic(MusicTrack track) {
    if (!m_initialized || !m_musicEnabled) return;
    const char* file = (track == MusicTrack::Main) ? "theme_main.ogg" : "theme_alt.ogg";
    if (Mix_PlayingMusic() && m_currentTrack == track) return;
    if (m_music) { Mix_FreeMusic(m_music); m_music = nullptr; }
    m_music = Mix_LoadMUS(sndPath(file).c_str());
    if (!m_music) { LOG_WARN("AudioManager: failed to load %s: %s", file, Mix_GetError()); return; }
    Mix_VolumeMusic(64);
    Mix_PlayMusic(m_music, -1);
    m_currentTrack = track;
}

void AudioManager::stopMusic() {
    if (!m_initialized) return;
    Mix_HaltMusic();
    if (m_music) { Mix_FreeMusic(m_music); m_music = nullptr; }
}

void AudioManager::setMusicEnabled(bool v) {
    m_musicEnabled = v;
    if (!v) stopMusic();
    else playMusic(m_currentTrack);
}

void AudioManager::setSoundEnabled(bool v) { m_soundEnabled = v; }
