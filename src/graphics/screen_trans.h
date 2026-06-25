#pragma once
#include <SDL.h>
#include "core/RAII.h"
#include "graphics/fader.h"
#include "core/Iplayable.h"

class ScreenTrans : public Iplayable
{
public:
    ScreenTrans(SDL_Renderer* ren, int w, int h)
    {
        SDL_Texture* tex = SDL_CreateTexture(
            ren, SDL_PIXELFORMAT_ARGB8888,
            SDL_TEXTUREACCESS_TARGET, w, h);

        SDL_SetRenderTarget(ren, tex);
        SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
        SDL_RenderFillRect(ren, nullptr);
        SDL_SetRenderTarget(ren, nullptr);

        SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
        shade.reset(tex);
    }

    void FadeIn(double durationSec)  { alphaFader.fadeIn(durationSec); }
    void FadeOut(double durationSec) { alphaFader.fadeOut(durationSec); }

    void SetOpaque()
    {
        alphaFader.fadeTo(1.0f, 0.0);
        SDL_SetTextureAlphaModFloat(shade.get(), 1.0f);
    }

    void SetTransparent()
    {
        alphaFader.fadeTo(0.0f, 0.0);
        SDL_SetTextureAlphaModFloat(shade.get(), 0.0f);
    }

    bool IsFading() const { return alphaFader.isActive(); }
    float GetAlpha() const { return static_cast<float>(alphaFader.Value()); }

    bool Update(double dt) override
    {
        float alpha = alphaFader.Update(dt);
        SDL_SetTextureAlphaModFloat(shade.get(), alpha);
        return alphaFader.isActive();
    }

    void Render(SDL_Renderer* ren, const SDL_FRect* dstRect = nullptr) override
    {
        float a = static_cast<float>(alphaFader.Value());
        if (a <= 0.0f) return;
        SDL_RenderTexture(ren, shade.get(), nullptr, nullptr);
    }

    bool IsPlaying() const override { return alphaFader.isActive(); }
    void Close() override { shade.reset(); }
    bool Update(SDL_Event& e) override { return false; }

private:
    Texture_ptr shade;
    Fader alphaFader{0.0};
};
