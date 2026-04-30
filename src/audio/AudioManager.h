#pragma once
#include <string>
#include <unordered_map>
#include <mutex>
#include <SDL2/SDL_mixer.h>

enum class SoundId {
    Navigate,
    DownloadStart,
    DownloadFinished,
    Error,
    ModActivated,
    ModDeactivated,
    Startup,
    ConflictWarning,
};

enum class MusicTrack { Main, Alt };

class AudioManager {
public:
    static AudioManager& get();
    void init();
    void shutdown();
    void playSound(SoundId id);
    void playMusic(MusicTrack track);
    void playMusicFadeIn(MusicTrack track, int fadeMs);
    void stopMusic();
    bool musicEnabled() const { return m_musicEnabled; }
    bool soundEnabled() const { return m_soundEnabled; }
    void setMusicEnabled(bool v);
    void setSoundEnabled(bool v);
private:
    AudioManager() = default;
    bool m_initialized  = false;
    bool m_musicEnabled = false;
    bool m_soundEnabled = true;
    MusicTrack m_currentTrack = MusicTrack::Main;
    std::unordered_map<int, Mix_Chunk*> m_sounds;
    Mix_Music* m_music = nullptr;
    std::mutex m_musicMutex; // protects m_music + m_currentTrack
};
