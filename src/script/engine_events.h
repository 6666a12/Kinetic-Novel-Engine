#pragma once
#include "script/text_decoder.h"
#include <functional>

namespace stdEngineEvents
{
    inline std::function<void(int)> gOnMusic;

    inline void OnFlash(int ms)          { (void)ms; }
    inline void OnFullscreen(int param)  { (void)param; }
    inline void OnPicture(int index)     { (void)index; }
    inline void OnMusic(int index)       { if (gOnMusic) gOnMusic(index); }
    inline void OnHide(int ms)           { (void)ms; }
    inline void OnEnding()               {}

    inline void Dispatch(const TextEvent& e)
    {
        switch (e.getEvent())
        {
            case EVENT::Flash:      OnFlash(e.getParam());      break;
            case EVENT::Fullscreen: OnFullscreen(e.getParam()); break;
            case EVENT::Pictures:   OnPicture(e.getParam());    break;
            case EVENT::music:      OnMusic(e.getParam());      break;
            case EVENT::hide:       OnHide(e.getParam());       break;
            case EVENT::ending:     OnEnding();                 break;
            default: break;
        }
    }

    void setVolume(double vol);
};
