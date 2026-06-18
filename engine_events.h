#pragma once
#include "text_decoder.h"

namespace stdEngineEvents
{
    void OnFlash(int ms) { (void)ms; }
    void OnFullscreen(int param) { (void)param; }
    void OnPicture(int index) { (void)index; }
    void OnMusic(int index) { (void)index; }
    void OnMusicPause() {}
    void OnHide(int ms) { (void)ms; }
    void OnEnding() {}

    void Dispatch(const TextEvent& e)
    {
        switch (e.getEvent())
        {
            case EVENT::Flash:
                OnFlash(e.getParam());
                break;

            case EVENT::Fullscreen:
                OnFullscreen(e.getParam());
                break;

            case EVENT::Pictures:
                OnPicture(e.getParam());
                break;

            case EVENT::musics:
                OnMusic(e.getParam());
                break;

            case EVENT::musicp:
                OnMusicPause();
                break;

            case EVENT::hide:
                OnHide(e.getParam());
                break;

            case EVENT::ending:
                OnEnding();
                break;
                
            default:
                break;
            }
        }

        
 
};
