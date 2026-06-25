#pragma once

#include <SDL.h>

class element
{
protected:
    SDL_FRect rect;
    bool ifcontain(float x, float y) const{
        return x >= rect.x && x <= rect.x + rect.w &&
               y >= rect.y && y <= rect.y + rect.h;
    }
    element(float x, float y, float w, float h)
        : rect{x, y, w, h} {}      

    void Notify(auto);
};