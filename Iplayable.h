#pragma once
#include <SDL.h>

class Iplayable
{
public:
    virtual ~Iplayable() = default;

    virtual bool Update(double dt) = 0;
    virtual void Render(SDL_Renderer* ren, const SDL_FRect* dstRect = nullptr) = 0;
    virtual bool IsPlaying() const = 0;
    virtual void Close() = 0;

};