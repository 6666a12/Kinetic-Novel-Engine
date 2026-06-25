#pragma once
#include <vector>
#include <memory>
#include <SDL.h>
#include "scene/iscene.h"
#include "graphics/screen_trans.h"

class SceneManager
{
public:
    SceneManager(SDL_Renderer* ren, int w, int h)
        : renderer(ren), transition(ren, w, h) {}

    void Push(std::unique_ptr<IScene> scene)
    {
        if (!stack.empty()) stack.back()->OnPause();
        scene->OnEnter();
        stack.push_back(std::move(scene));
    }

    void PushWithTransition(std::unique_ptr<IScene> scene, double durationSec = 0.4)
    {
        if (IsTransitioning()) return;
        transitionDuration = durationSec;
        pendingOp = PendingOp::Push;
        pendingScene = std::move(scene);

        transition.FadeIn(transitionDuration);
        transState = TransitionState::ToBlack;
    }

    void Pop() { if (!stack.empty()) { stack.back()->OnExit(); stack.pop_back(); if (!stack.empty()) stack.back()->OnResume(); } }

    void PopWithTransition(double durationSec = 0.4)
    {
        if (stack.empty() || IsTransitioning()) return;
        transitionDuration = durationSec;
        pendingOp = PendingOp::Pop;
        pendingScene.reset();

        transition.FadeIn(transitionDuration);
        transState = TransitionState::ToBlack;
    }

    void Replace(std::unique_ptr<IScene> scene)
    {
        if (stack.empty()) { Push(std::move(scene)); return; }
        stack.back()->OnExit();
        stack.pop_back();
        scene->OnEnter();
        stack.push_back(std::move(scene));
    }

    void ReplaceWithTransition(std::unique_ptr<IScene> scene, double durationSec = 0.4)
    {
        if (stack.empty() || IsTransitioning()) return;
        transitionDuration = durationSec;
        pendingOp = PendingOp::Replace;
        pendingScene = std::move(scene);

        transition.FadeIn(transitionDuration);
        transState = TransitionState::ToBlack;
    }

    void HandleEvent(const SDL_Event& e)
    {
        if (IsTransitioning()) return;
        if (!stack.empty()) stack.back()->HandleEvent(e);
    }

    void Update(double dt)
    {
        if (IsTransitioning())
        {
            transition.Update(dt);

            if (transState == TransitionState::ToBlack && !transition.IsFading())
            {
                transState = TransitionState::Swapping;
                switch (pendingOp)
                {
                    case PendingOp::Push:    if (pendingScene) Push(std::move(pendingScene)); break;
                    case PendingOp::Pop:     Pop(); break;
                    case PendingOp::Replace: if (pendingScene) Replace(std::move(pendingScene)); break;
                    default: break;
                }
                pendingOp = PendingOp::None;

                transition.FadeOut(transitionDuration);
                transState = TransitionState::FromBlack;
            }

            if (transState == TransitionState::FromBlack && !transition.IsFading())
                transState = TransitionState::Idle;

            return;
        }

        if (stack.empty()) return;
        stack.back()->Update(dt);
        ProcessSceneRequest();
    }

    void Render(SDL_Renderer* ren)
    {
        for (size_t i = 0; i < stack.size(); ++i)
        {
            stack[i]->Render(ren);
            if (stack[i]->IsOpaque()) break;
        }

        if (IsTransitioning())
            transition.Render(ren, nullptr);
    }

    bool IsEmpty() const { return stack.empty(); }
    bool IsTransitioning() const { return transState != TransitionState::Idle; }

private:
    std::vector<std::unique_ptr<IScene>> stack;
    SDL_Renderer* renderer;
    ScreenTrans transition;

    enum class TransitionState { Idle, ToBlack, Swapping, FromBlack };
    enum class PendingOp { None, Push, Pop, Replace };

    TransitionState transState = TransitionState::Idle;
    PendingOp pendingOp = PendingOp::None;
    std::unique_ptr<IScene> pendingScene;
    double transitionDuration = 0.4;

    void ProcessSceneRequest()
    {
        if (stack.empty()) return;
        IScene::Request req = stack.back()->GetRequest();
        auto nextScene = stack.back()->TakeRequestedScene();

        switch (req)
        {
            case IScene::Request::None: break;

            case IScene::Request::Push:
                if (nextScene) PushWithTransition(std::move(nextScene));
                break;

            case IScene::Request::Pop:
                PopWithTransition();
                break;

            case IScene::Request::Replace:
                if (nextScene) ReplaceWithTransition(std::move(nextScene));
                break;

            case IScene::Request::Quit:
                while (!stack.empty()) Pop();
                break;
        }
    }
};
