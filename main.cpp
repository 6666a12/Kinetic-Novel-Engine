/*
    Visual Novel Project based on SDL 3.4.8
    You can called it an engine(??.)

    All the resource of the project:
        1. Text
        2. Audio
        3. Textures:
            UI design
            Background
            Videos (?)
            Font using stb_truetype
        4. Surface
            Screenshot
            Some geometric shapes  

    Special Thanks: CHATGPT, KIMI and DEEPSEEK..
*/

#define _RELEASE
#define STB_IMAGE_IMPLEMENTATION

#ifdef _RELEASE
    #pragma GCC optimize ("O2")
#else
    #pragma GCC optimize ("O0")
#endif

#include <SDL_ttf.h>
#include <SDL_main.h>
#include <cctype>
#include <cmath>
#include <fstream>
#include <functional>
#include <algorithm>
#include <variant>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include "third-party/glad/glad.h"
#include "core/RAII.h"
#include "core/curve.h"
#include "core/Iplayable.h"
#include "data/backup.h"
#include "graphics/api.h"
#include "graphics/Ifadable.h"
#include "audio/audio_manager.h"
#include "video/video_player.h"
#include "ui/UI_component.h"
#include "script/text_decoder.h"
#include "script/engine_events.h"
#include "scene/scene_manager.h"

#define TITLE "Demo"
#define WIDTH 1920
#define HEIGHT 1080

/*
    freq: 44800hz
    channel: 2
    format: SDL_AUDIO_S16LE
*/
SDL_AudioSpec spec = {
    SDL_AUDIO_S16LE,
    2,
    44800
};


std::unordered_map<std::string, SDL_Color> color_map;

void color_init(std::unordered_map<std::string, SDL_Color> *color_map);

class textbox
{
protected:
    std::string str;
    const std::function<double(double)> curv;

};

/*
    void Render_selectbox(SDL_Renderer *ren, SDL_Color color)
    {   
        if(this->pressed){
            SDL_SetRenderDrawColor(ren, color.r, color.g, color.b, color.a);
        }
    }
*/

#include "scene/title_scene.h"
#include "scene/playing_scene.h"




int main(int argc, char* argv[])
{
    TTF_Init();
    TTF_Font* fon = TTF_OpenFont("font.ttf", 28);
    if (!fon) fon = TTF_OpenFont("C:/Windows/Fonts/msyh.ttc", 28);
    if (!fon) fon = TTF_OpenFont("C:/Windows/Fonts/simhei.ttf", 28);
    if (!fon) fon = TTF_OpenFont("C:/Windows/Fonts/arial.ttf", 28);
    Font_ptr font(fon);

    SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO);

    SDL_AudioDeviceID dev = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec);

    AudioManager audioManager(dev);

    stdEngineEvents::gOnMusic = [&](int index) {
        if (index == 0) audioManager.StopBGM(0.8);
    };

    SDL_AudioSpec specStream = { SDL_AUDIO_S16LE, 2, 44800 };
    SDL_AudioStream* videoStream = SDL_CreateAudioStream(&specStream, &specStream);
    SDL_BindAudioStream(dev, videoStream);
    
    // Window Init
    SDL_Window *wind = SDL_CreateWindow(TITLE, WIDTH, HEIGHT, 0);
    scc(wind, "Window Initialize failed:");
    Window_ptr win(wind);

    // Renderer Init
    SDL_Renderer *rend = SDL_CreateRenderer(win.get(), NULL);
    scc(rend, "Renderer Initialize failed:");
    SDL_SetRenderDrawBlendMode(rend, SDL_BLENDMODE_BLEND);
    SDL_SetRenderVSync(rend, 1);
    Renderer_ptr ren(rend);

    SceneManager sceneManager(rend, WIDTH, HEIGHT);
    sceneManager.Push(std::make_unique<TitleScene>(ren.get(), font.get(), dev, videoStream));

    Uint64 lastTick = SDL_GetTicks();
    SDL_Event e;
    bool running = true;

    while (running)
    {
        while (SDL_PollEvent(&e))
        {
            if (e.type == SDL_EVENT_QUIT) { running = false; break; }
            sceneManager.HandleEvent(e);
        }
        if (sceneManager.IsEmpty()) running = false;
        if (!running) break;

        Uint64 nowTick = SDL_GetTicks();
        double dt = (nowTick - lastTick) / 1000.0;
        lastTick = nowTick;

        sceneManager.Update(dt);
        audioManager.Update(dt);

        SDL_SetRenderDrawColor(ren.get(), 0, 0, 0, 255);
        SDL_RenderClear(ren.get());
        sceneManager.Render(ren.get());
        SDL_RenderPresent(ren.get());
    }
    
    audioManager.Close();
    SDL_DestroyAudioStream(videoStream);
    SDL_Quit();
    return 0;
}




