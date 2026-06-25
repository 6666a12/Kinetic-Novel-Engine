#pragma once

#include <SDL.h>
#include <functional>
#include <string>
#include "core/RAII.h"
#include "core/Iplayable.h"
#include "ui/element.h"

// Class Selectbox
class button : public element
{
protected:
    // SDL settings for the box
    SDL_Color defaultbg;

    // Font for the text
    Font_ptr font;

    // Background texture of the button
    Texture_ptr bgtext;

public:
    explicit button(float x, float y, float w, float h, SDL_Color color)
        : element{x, y, w, h}, defaultbg(color){}

    void setFont(TTF_Font* font1){
        if(!font1) return;
        Font_ptr font0(font1);
        font = std::move(font0);
    }

    void setText(SDL_Texture* texture){
        if(!texture) return;
        Texture_ptr t0(texture);
        bgtext = std::move(t0);
    }
    
};


// Single choice VER.
class buttonS : public button, public Iplayable
{
protected:
    // Content of the box
    std::variant<
        std::string, Texture_ptr
    > content;

    std::string textLabel;

    enum struct status{
        none = 0,
        hovered = 1,
        pressed = 2,
        chosen = 3
    };

    status state = status::none;

    std::function<void(bool)> onChange;
    bool* boundValue = nullptr;

    void Notify(status newState)
    {
        if (state == newState) return;
        state = newState;

        bool isChosen = (state == status::chosen);
        if (boundValue) *boundValue = isChosen;
        if (onChange)   onChange(isChosen);
    }

public:
    buttonS(float x, float y, float w, float h, SDL_Color color):
        button{x, y, w, h, color} {}

    bool SetString(TTF_Font* font1, SDL_Renderer* ren, const std::string& str)
    {
        if(!font1 || !ren) return false;
        textLabel = str.empty() ? textLabel : str;

        SDL_Surface* s = TTF_RenderText_Blended(font1, textLabel.c_str(), textLabel.size(), {255, 255, 255, 255});
        if(!s) return false;

        SDL_Texture* texture0 = SDL_CreateTextureFromSurface(ren, s);
        content = Texture_ptr(std::move(texture0));
        SDL_DestroySurface(s);
        return true;
    }

    bool Update(SDL_Event& e) override
    {
        if(e.type == SDL_EVENT_MOUSE_MOTION && ifcontain(e.motion.x, e.motion.y)){
            Notify(status::hovered);
        }
        else if(ifcontain(e.button.x, e.button.y)){
            if(e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_LEFT && this->state == status::hovered)
                Notify(status::pressed);
            else if(e.type == SDL_EVENT_MOUSE_BUTTON_UP && e.button.button == SDL_BUTTON_LEFT && this->state == status::pressed)
                Notify(status::chosen);
        }else if(!ifcontain(e.button.x, e.button.y)){
            Notify(status::none);
        }
        return false;
    }

    bool Update(double dt) override { return false; }
    bool IsPlaying() const override { return true; }
    void Close() override { return; }

    void SetOnChange(std::function<void(bool)> callback){ this->onChange = std::move(callback); }
    void BindValue(bool* data)
    {
        this->boundValue = data;
        if (boundValue) *boundValue = (state == status::chosen);
    }

    void Render(SDL_Renderer* ren, const SDL_FRect* dstRect = nullptr) override
    {
        SDL_FRect r = dstRect ? *dstRect : rect;

        SDL_Color c = defaultbg;
        switch (state)
        {
            case status::hovered:
                c.r = std::min(255, c.r + 50);
                c.g = std::min(255, c.g + 50);
                c.b = std::min(255, c.b + 50);
                break;
            case status::pressed:
                c.r = (Uint8)(c.r * 0.7f);
                c.g = (Uint8)(c.g * 0.7f);
                c.b = (Uint8)(c.b * 0.7f);
                break;
            default:
                break;
        }
        SDL_SetRenderDrawColor(ren, c.r, c.g, c.b, c.a);
        SDL_RenderFillRect(ren, &r);

        SDL_SetRenderDrawColor(ren, 180, 180, 200, 120);
        SDL_RenderRect(ren, &r);

        if (std::holds_alternative<Texture_ptr>(content))
        {
            const auto& tex = std::get<Texture_ptr>(content);
            if (tex)
            {
                float tw = 0, th = 0;
                SDL_GetTextureSize(tex.get(), &tw, &th);
                SDL_FRect td = {r.x + (r.w - tw) * 0.5f, r.y + (r.h - th) * 0.5f, tw, th};
                SDL_RenderTexture(ren, tex.get(), nullptr, &td);
            }
        }
    }
};

// Multiple choice VER.
class buttonM : public button
{
protected:
    // Contents of the box
    std::vector<std::variant<
        std::string, Texture_ptr
    >> Contents;

    enum struct status{
        none = 0,
        hovered = 1,
        pressed = 2,
    };

    status state = status::none;

    /*
        The index of contents the mouse is put on 
            empty - -1
            contents - 1 ~ n
    */
    int index = -1;

    std::function<void(int)> onChange;
    int* boundValue = nullptr;

    

    void Notify(int newIndex)
    {
        if (index == newIndex || index == -1) return;
        index = newIndex;
        if (boundValue) *boundValue = index;
        if (onChange)   onChange(index);
    }

public:
    buttonM(float x, float y, float w, float h, SDL_Color color, int index)
        :button{x, y, w, h, color}, index(index) {}

    void SetOnChange(std::function<void(int)> callback){ this->onChange = std::move(callback); }
    void BindValue(int* data)
    {
        this->boundValue = data;
        if (boundValue) *boundValue = index;
    }


};