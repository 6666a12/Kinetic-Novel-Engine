#pragma once
#include <SDL.h>
#include <memory>

class IScene
{
public:
    virtual ~IScene() = default;

    // Lifecycle
    virtual void OnEnter() {}
    virtual void OnExit() {}
    virtual void OnPause() {}
    virtual void OnResume() {}

    // Per-frame
    virtual bool Update(double dt) = 0;
    virtual bool HandleEvent(const SDL_Event& e) = 0;
    virtual void Render(SDL_Renderer* ren) = 0;

    // Optimization: skip rendering layers below an opaque full-screen scene
    virtual bool IsOpaque() const { return false; }

    // Scene transition requests (scenes don't directly manipulate SceneManager)
    enum class Request { None, Push, Pop, Replace, Quit };

    virtual Request GetRequest() const { return Request::None; }
    virtual std::unique_ptr<IScene> TakeRequestedScene() { return nullptr; }
};
