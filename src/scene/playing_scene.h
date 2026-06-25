#pragma once

#include <SDL_ttf.h>
#include "scene/iscene.h"
#include "core/Iplayable.h"
#include "ui/text_area.h"
#include "video/video_player.h"

class PlayingScene : public IScene
{
public:
    PlayingScene(SDL_Renderer* ren, TTF_Font* font, SDL_AudioDeviceID dev, SDL_AudioStream* vStream, int chapterId = 1)
        : renderer(ren), audioDevice(dev), videoStream(vStream)
    {
        (void)chapterId;
        textArea.SetFont(font);
        textArea.SetArea(SDL_FRect{120.0f, 760.0f, 1680.0f, 240.0f});
        textArea.SetSpeed(36.0);
    }

    void OnEnter() override
    {
        stdData demoData;
        Text demoPage1;
        demoPage1.speaker = "Narrator";
        demoPage1.eventList.emplace_back(std::string("这是 TextArea 的逐字渲染测试。按 Space 可以暂停或继续，点击文本框可以跳过当前页。"));
        demoPage1.eventList.emplace_back(std::vector<TextEvent>{TextEvent(EVENT::Pictures, 1), TextEvent(EVENT::hide, 1)});
        demoPage1.eventList.emplace_back(std::string("事件会按照文本中的顺序触发，然后继续显示后面的文字。"));
        demoData[{1, 1}] = std::move(demoPage1);

        Text demoPage2;
        demoPage2.speaker = "System";
        demoPage2.eventList.emplace_back(std::string("第二页用于验证翻页。页面结束后，左方向键回到上一页，右方向键进入下一页。按 V 仍然可以切换视频播放。"));
        demoData[{1, 2}] = std::move(demoPage2);

        textArea.LoadData(demoData, {1, 1});
    }

    bool Update(double dt) override
    {
        textArea.Update(dt);
        player.Update(dt);
        return true;
    }

    bool HandleEvent(const SDL_Event& e) override
    {
        if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_ESCAPE)
        {
            currentRequest = Request::Pop;
            return true;
        }
        player.HandleEvent("霜月はるか,riya - 廻る世界で.mp4", e, videoStream, renderer);
        textArea.HandleEvent(e);
        return true;
    }

    void Render(SDL_Renderer* ren) override
    {
        SDL_SetRenderDrawColor(ren, 8, 8, 16, 255);
        SDL_RenderClear(ren);
        if (player.IsPlaying()) player.Render(ren);
        if (textArea.IsPlaying()) textArea.Render(ren);
    }

    bool IsOpaque() const override { return true; }

    Request GetRequest() const override { return currentRequest; }
    std::unique_ptr<IScene> TakeRequestedScene() override { return std::move(nextScene); }

private:
    SDL_Renderer* renderer;
    SDL_AudioDeviceID audioDevice;
    SDL_AudioStream* videoStream;
    TextArea textArea;
    VideoPlayer player;
    Request currentRequest = Request::None;
    std::unique_ptr<IScene> nextScene;
};
