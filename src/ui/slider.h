#pragma once

#include <SDL.h>
#include <functional>
#include <string>
#include "core/RAII.h"
#include "core/Iplayable.h"
#include "ui/element.h"

// <<---------o--------->
class slide : public element, public Iplayable
{
protected:
    // Min and Max value of toggle
    double min, max;

    // Current position of toggle
    double pos;

    // Content of the toggle
    std::string str;

    // Only 1 param
    bool toggled = false;

    // Value and Function binded to the toggle
    double val;
    std::function<void(double)> onChange;
    double* boundValue = nullptr;

    // Font for the text
    Font_ptr font;

    // Background texture of the button
    Texture_ptr bgtext;

    void Notify()
    {
        if (boundValue) *boundValue = val;
        if (onChange)   onChange(val);
    }

public:
    explicit slide(double x, double y, double w, double h, const std::string& str0, double min, double max)
        : element{x, y, w, h}, str(str0), min(min), max(max)
    {
        this->pos = this->rect.x;
        this->val = this->getval();
    }
 
    void setFont(TTF_Font* font1){
        if(!font1) return;
        Font_ptr font0(font1);
        font = std::move(font0);
    }

    void setText(SDL_Texture* texture, SDL_Renderer* ren){
        Texture_ptr t0;
        if(!texture){
            SDL_Surface* s;
            FillCircleAA(s, 0, 0, 50, 255, 255, 255);
            SDL_Texture* tex0 = SDL_CreateTextureFromSurface(ren, s);
            SDL_DestroySurface(s);
            t0 = Texture_ptr(tex0);
        }else{
            t0 = Texture_ptr(texture);
        } 
        
        bgtext = std::move(t0);
    }

    double getval() const { return this->min + (this->max - this->min) * (this->pos - this->rect.x) / this->rect.w; }

    // Bind the toggle with a specific data or callback
    void SetOnChange(std::function<void(double)> callback){ this->onChange = std::move(callback); }
    void BindValue(double* data)
    {
        this->boundValue = data;
        if (boundValue) *boundValue = val;
    }

    // Just a conclusion of repeated codes
    void changex(float mx){
        if(mx > this->rect.x + this->rect.w) this->pos = this->rect.x + this->rect.w;
        else if(mx < this->rect.x) this->pos = this->rect.x;
        else this->pos = mx;

        double newVal = this->getval();
        if (newVal != this->val) {
            this->val = newVal;
            Notify();
        }
    }

    // @param e SDL event
    bool Update(SDL_Event& e) override
    {
        if(e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_LEFT)
        {
            this->toggled = this->ifcontain(e.button.x, e.button.y);
            if(this->toggled) changex(static_cast<float>(e.button.x));
        }
        else if(e.type == SDL_EVENT_MOUSE_BUTTON_UP && e.button.button == SDL_BUTTON_LEFT)
        {
            this->toggled = false;
        }
        else if(e.type == SDL_EVENT_MOUSE_MOTION && this->toggled)
        {
            changex(static_cast<float>(e.motion.x));
        }
        else
        {
            return false;
        }

        return true;
    }


    /*
        @param t Central button texture
        @param ren SDL renderer
    */
    void Render(SDL_Renderer* ren, const SDL_FRect* dstRect = nullptr) override
    {
        SDL_FRect dst0Rect = {
            static_cast<float>(this->pos), this->rect.y + 2,
            this->rect.h - 4, this->rect.h - 4
        };

        SDL_RenderRect(ren, &this->rect);

        const SDL_FRect* dst0 = dstRect ? dstRect : &dst0Rect;
        SDL_RenderTexture(ren, bgtext.get(), NULL, dst0);
    }

    bool IsPlaying() const override { return true; }

    void Close() override { return; }
};