#pragma once
#include <SDL.h>
#include "audio/audio_channel.h"

class AudioManager
{
public:
    static constexpr int SFX_POOL_SIZE = 8;

    AudioManager(SDL_AudioDeviceID device)
        : audioDevice(device) {}

    ~AudioManager() { Close(); }

    bool PlayBGM(const char* path, double fadeInSec = 0.6, bool loop = true)
    {
        if (!path || !*path) return false;

        if (ActiveBGM().IsPlaying())
        {
            return CrossfadeBGM(path, 1.2);
        }

        AudioChannel& ch = ActiveBGM();
        ch.SetGain(0.0f);
        if (!ch.Play(path, audioDevice, loop)) return false;
        ch.FadeGainTo(bgmVolume, fadeInSec);
        bgmPlaying = true;
        bgmPath = path;
        return true;
    }

    void StopBGM(double fadeOutSec = 0.9)
    {
        ActiveBGM().FadeGainTo(0.0f, fadeOutSec);
        bgmPlaying = false;
    }

    bool CrossfadeBGM(const char* path, double crossfadeSec = 1.2)
    {
        if (!path || !*path) return false;

        AudioChannel& oldCh = ActiveBGM();
        AudioChannel& newCh = IdleBGM();

        newCh.SetGain(0.0f);
        if (!newCh.Play(path, audioDevice, true)) return false;
        newCh.FadeGainTo(bgmVolume, crossfadeSec);

        oldCh.FadeGainTo(0.0f, crossfadeSec);

        useChannelA = !useChannelA;
        bgmPlaying = true;
        bgmPath = path;
        return true;
    }

    void SetBGMVolume(float vol)
    {
        bgmVolume = std::clamp(vol, 0.0f, 1.0f);
        if (bgmPlaying) ActiveBGM().SetGain(bgmVolume);
    }

    bool PlayVoice(const char* path, double fadeInSec = 0.05)
    {
        voiceChannel.Stop(0.1);
        voiceChannel.SetGain(0.0f);
        if (!voiceChannel.Play(path, audioDevice, false)) return false;
        voiceChannel.FadeGainTo(voiceVolume, fadeInSec);
        return true;
    }

    void StopVoice(double fadeOutSec = 0.12)
    {
        voiceChannel.Stop(fadeOutSec);
    }

    bool IsVoicePlaying() const { return voiceChannel.IsPlaying(); }
    void SetVoiceVolume(float vol) { voiceVolume = std::clamp(vol, 0.0f, 1.0f); }

    bool PlaySFX(const char* path)
    {
        AudioChannel& sfx = sfxPool[sfxCursor];
        sfxCursor = (sfxCursor + 1) % SFX_POOL_SIZE;

        sfx.Stop(0.0);
        sfx.SetGain(sfxVolume);
        return sfx.Play(path, audioDevice, false);
    }

    void SetSFXVolume(float vol) { sfxVolume = std::clamp(vol, 0.0f, 1.0f); }

    void SetMasterVolume(float vol)
    {
        masterVolume = std::clamp(vol, 0.0f, 1.0f);
        SDL_SetAudioDeviceGain(audioDevice, masterVolume);
    }

    float GetMasterVolume() const { return masterVolume; }

    void Update(double dt)
    {
        bgmChannelA.Update(dt);
        bgmChannelB.Update(dt);
        voiceChannel.Update(dt);

        for (auto& sfx : sfxPool)
            sfx.Update(dt);
    }

    void Close()
    {
        bgmChannelA.Stop(0.0);
        bgmChannelB.Stop(0.0);
        voiceChannel.Stop(0.0);
        for (auto& sfx : sfxPool)
            sfx.Stop(0.0);
        bgmPlaying = false;
    }

private:
    SDL_AudioDeviceID audioDevice;

    AudioChannel bgmChannelA;
    AudioChannel bgmChannelB;
    bool useChannelA = true;
    bool bgmPlaying = false;

    AudioChannel voiceChannel;
    AudioChannel sfxPool[SFX_POOL_SIZE];
    int sfxCursor = 0;

    float masterVolume = 1.0f;
    float bgmVolume = 1.0f;
    float voiceVolume = 1.0f;
    float sfxVolume = 1.0f;

    std::string bgmPath;

    AudioChannel& ActiveBGM()  { return useChannelA ? bgmChannelA : bgmChannelB; }
    AudioChannel& IdleBGM()    { return useChannelA ? bgmChannelB : bgmChannelA; }
};
