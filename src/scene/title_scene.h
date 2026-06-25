#pragma once
#include "scene/iscene.h"
#include "ui/UI_component.h"
#include "core/RAII.h"
#include "scene/playing_scene.h"

class TitleScene : public IScene
{
public:
    static constexpr float BTN_W = 320.0f;
    static constexpr float BTN_H = 64.0f;
    static constexpr float BTN_X = (1920.0f - BTN_W) * 0.5f;

    TitleScene(SDL_Renderer* ren, TTF_Font* font, SDL_AudioDeviceID dev, SDL_AudioStream* vStream)
        : renderer(ren), fontPtr(font), audioDevice(dev), videoStream(vStream),
          btnStart(BTN_X, 380, BTN_W, BTN_H, SDL_Color{35, 35, 55, 230}),
          btnSettings(BTN_X, 474, BTN_W, BTN_H, SDL_Color{35, 35, 55, 230}),
          btnExit(BTN_X, 568, BTN_W, BTN_H, SDL_Color{35, 35, 55, 230})
    {
        btnStart.SetString(font, ren, "开始游戏");
        btnSettings.SetString(font, ren, "设置");
        btnExit.SetString(font, ren, "退出游戏");

        btnStart.SetOnChange([this](bool c) {
            if (c) {
                currentRequest = Request::Push;
                nextScene = std::make_unique<PlayingScene>(renderer, fontPtr, audioDevice, videoStream, 1);
            }
        });

        btnExit.SetOnChange([this](bool c) {
            if (c) currentRequest = Request::Quit;
        });
    }

    bool Update(double dt) override { return true; }

    bool HandleEvent(const SDL_Event& e) override
    {
        SDL_Event ev = e;
        btnStart.Update(ev);
        btnSettings.Update(ev);
        btnExit.Update(ev);
        return true;
    }

    void Render(SDL_Renderer* ren) override
    {
        SDL_SetRenderDrawColor(ren, 8, 8, 16, 255);
        SDL_RenderFillRect(ren, nullptr);
        btnStart.Render(ren);
        btnSettings.Render(ren);
        btnExit.Render(ren);
    }

    bool IsOpaque() const override { return true; }

    Request GetRequest() const override { return currentRequest; }
    std::unique_ptr<IScene> TakeRequestedScene() override { return std::move(nextScene); }

private:
    SDL_Renderer* renderer;
    TTF_Font* fontPtr;
    SDL_AudioDeviceID audioDevice;
    SDL_AudioStream* videoStream;
    buttonS btnStart, btnSettings, btnExit;
    Request currentRequest = Request::None;
    std::unique_ptr<IScene> nextScene;
};
