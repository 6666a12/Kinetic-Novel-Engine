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
#include "stb_image.h"
#include "RAII.h"
#include "Iplayable.h"
#include "backup.h"
#include "video_player.h"
#include "text_decoder.h"
#include "engine_events.h"
#include "UI_component.h"

#define TITLE "Demo"
#define WIDTH 1920
#define HEIGHT 1080

// <======================== Definition ====================================

void screenshot(SDL_Renderer*, char*);
SDL_Surface* LoadImg2Surf(const char* path);
void unpackRGBA(Uint32 pixel, Uint8 &r, Uint8 &g, Uint8 &b, Uint8 &a);
Uint32 packRGBA(Uint8 r, Uint8 g, Uint8 b, Uint8 a);
void BlendPixel(SDL_Surface* surf, int x, int y, Uint8 r, Uint8 g, Uint8 b, float a);
void FillCircleAA(SDL_Surface* surf, float cx, float cy, float r, Uint8 cr, Uint8 cg, Uint8 cb);

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



/*
    Screen Transition Effect
    @param fade TRUE for fadeout 
                FALSE for fadein
*/
class ScreenTrans
{
protected:
    Texture_ptr shade;
    float alpha = 0.0f;
    bool fade;

public:
    ScreenTrans(SDL_Renderer* ren, int w, int h){
        SDL_Texture* shades = shade.get();
        shades = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, w, h);
        SDL_SetRenderTarget(ren, shades);
        SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
        SDL_RenderFillRect(ren, nullptr);
        SDL_SetRenderTarget(ren, nullptr);
        SDL_SetTextureBlendMode(shades, SDL_BLENDMODE_BLEND);
    }

    void startFadeOut(){ fade = true, alpha = 0.0f; }
    void startFadeIn(){ fade = false, alpha = 255.0f; }

    bool update(double dt, double speed = 400.0f)
    {
        double t = dt * speed;
        this->alpha += t * (this->fade ? 1 : -1);
        if(this->fade && alpha >= 255.0f){ alpha = 255.0f; return true; }
        else if(!this->fade && alpha <= 0.0f){ alpha = 0.0f; return true; }
        SDL_SetTextureAlphaModFloat(this->shade.get(), alpha);
        return false; 
    }

    void render(SDL_Renderer* ren){
        if(alpha <= 0.0f) return;
        SDL_RenderTexture(ren, this->shade.get(), nullptr, nullptr);
    }
};

// Manager for INITIAL texture of the process
class Assetmanager
{
protected:
    std::unordered_map<std::string, Texture_ptr> textureCache;

public:
    // Default Empty Initializer
    Assetmanager() = default;
    // init

    SDL_Texture* GetTexture(std::string tag){ return this->textureCache[tag] ? this->textureCache[tag].get() : nullptr; }

    // Manually Append.
    void append(std::string tag, SDL_Texture *tex){
        Texture_ptr t(tex);
        this->textureCache[tag] = std::move(t);
    }

    // Manually remove.
    void remove(std::string tag){
        SDL_Texture *t = this->textureCache[tag].release();
        SDL_DestroyTexture(t);
    }

    void init() {}
};

void color_init(std::unordered_map<std::string, SDL_Color> *color_map);

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

// Class Selectbox
class box : public element
{
protected:
    // SDL settings for the box
    SDL_Color defaultbg;

public:
    explicit box(float x, float y, float w, float h, SDL_Color color)
        : element{x, y, w, h}, defaultbg(color){}
    
};


// Single choice VER.
class boxS : public box
{
protected:
    // Content of the box
    std::variant<
        std::string, Texture_ptr
    > content;

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
    boxS(float x, float y, float w, float h, SDL_Color color):
        box{x, y, w, h, color} {}

    void handle(SDL_Event e)
    {
        if(e.type == SDL_EVENT_MOUSE_MOTION && ifcontain(e.motion.x, e.motion.y)){
            Notify(status::hovered);
        }
        else if(ifcontain(e.button.x, e.button.y)){
            if(e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_LEFT && this->state == status::hovered)
                Notify(status::pressed);
            else if(e.type == SDL_EVENT_MOUSE_BUTTON_UP && e.button.button == SDL_BUTTON_LEFT && this->state == status::pressed)
                Notify(status::chosen);
        }
    }

    void SetOnChange(std::function<void(bool)> callback){ this->onChange = std::move(callback); }
    void BindValue(bool* data)
    {
        this->boundValue = data;
        if (boundValue) *boundValue = (state == status::chosen);
    }
};

// Multiple choice VER.
class boxM : public box
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
    boxM(float x, float y, float w, float h, SDL_Color color, int index)
        :box{x, y, w, h, color}, index(index) {}

    void SetOnChange(std::function<void(int)> callback){ this->onChange = std::move(callback); }
    void BindValue(int* data)
    {
        this->boundValue = data;
        if (boundValue) *boundValue = index;
    }

};

// <<---------o--------->
class toggle : public element
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

    void Notify()
    {
        if (boundValue) *boundValue = val;
        if (onChange)   onChange(val);
    }

public:
    explicit toggle(double x, double y, double w, double h, const std::string& str0, double min, double max)
        : element{x, y, w, h}, str(str0), min(min), max(max)
    {
        this->pos = this->rect.x;
        this->val = this->getval();
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
    void handle(const SDL_Event& e){
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
    }

    /*
        @param t Central button texture
        @param ren SDL renderer
    */
    void Render(SDL_Texture* t, SDL_Renderer* ren, TTF_Font* font)
    {
        SDL_FRect dstRect = {
            static_cast<float>(this->pos), this->rect.y + 2,
            this->rect.h - 4, this->rect.h - 4
        };

        SDL_RenderRect(ren, &this->rect);
        SDL_RenderTexture(ren, t, NULL, &dstRect);
    }
};

// SIZE: HEIGHT * WIDTH FIXED
class RenderPage
{
protected:
    std::vector<element> UI_elements;
    int mindex = -1;
    std::string tindex;

public:


};

/*
    void Render_selectbox(SDL_Renderer *ren, SDL_Color color)
    {   
        if(this->pressed){
            SDL_SetRenderDrawColor(ren, color.r, color.g, color.b, color.a);
        }
    }
*/


int main(int argc, char* argv[])
{
    TTF_Init();
    TTF_Font* fon = TTF_OpenFont("font.ttf", 28);
    if (!fon) fon = TTF_OpenFont("C:/Windows/Fonts/msyh.ttc", 28);
    if (!fon) fon = TTF_OpenFont("C:/Windows/Fonts/simhei.ttf", 28);
    if (!fon) fon = TTF_OpenFont("C:/Windows/Fonts/arial.ttf", 28);
    Font_ptr font(fon);

    Assetmanager Assets = Assetmanager();
    Assets.init();
    
    SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO);

    SDL_AudioDeviceID dev = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec);
    SDL_AudioStream *aus = SDL_CreateAudioStream(&spec, &spec);
    scc(SDL_BindAudioStream(dev, aus), "Audio device bind failed:");
    
    // Window Init
    SDL_Window *wind = SDL_CreateWindow(TITLE, 1920, 1080, 0);
    scc(wind, "Window Initialize failed:");
    Window_ptr win(wind);

    // Renderer Init
    SDL_Renderer *rend = SDL_CreateRenderer(win.get(), NULL);
    scc(rend, "Renderer Initialize failed:");
    SDL_SetRenderDrawBlendMode(rend, SDL_BLENDMODE_BLEND);
    SDL_SetRenderVSync(rend, 1);
    Renderer_ptr ren(rend);

    TextArea textArea;
    textArea.SetFont(font.get());
    textArea.SetArea(SDL_FRect{120.0f, 760.0f, 1680.0f, 240.0f});
    textArea.SetSpeed(36.0);

// TextArea demodata
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

    VideoPlayer player;
    Uint64 lastTick = SDL_GetTicks();
    
    SDL_Event e;
    bool running = true;

    while( running ){
        while(SDL_PollEvent(&e)){
            if (e.type == SDL_EVENT_QUIT){
                running = false;
                break;
            }

            TextArea::Action textAction = textArea.HandleEvent(e);
            if (textAction != TextArea::Action::None)
            {
                continue;
            }

            if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_V)
            {
                if (!player.IsPlaying())
                {
                    player.Open("霜月はるか,riya - 廻る世界で.mp4", ren.get(), aus);
                }
                else
                {
                    player.Close();
                }
            }
        }

        Uint64 nowTick = SDL_GetTicks();
        double dt = (nowTick - lastTick) / 1000.0;
        lastTick = nowTick;

        textArea.Update(dt);

        SDL_SetRenderDrawColor(ren.get(), 0, 0, 0, 255);
        SDL_RenderClear(ren.get());

        if (player.IsPlaying())
        {
            player.Update(dt);
            player.Render(ren.get());
        }

        textArea.Render(ren.get());
        SDL_RenderPresent(ren.get());

    }

    player.Close();
    SDL_DestroyAudioStream(aus);
    SDL_Quit();
    return 0;
}



void unpackRGBA(Uint32 pixel, Uint8 &r, Uint8 &g, Uint8 &b, Uint8 &a)
{
    r = (pixel >> 24) & 0xFF;
    g = (pixel >> 16) & 0xFF;
    b = (pixel >> 8 ) & 0xFF;
    a = (pixel >> 0 ) & 0xFF;
}

Uint32 packRGBA(Uint8 r, Uint8 g, Uint8 b, Uint8 a){ return ((Uint32)r << 24 | (Uint32)g << 16 | (Uint32)b << 8 | (Uint32)a ); }

void screenshot(SDL_Renderer *ren, char* path)
{
    SDL_Surface *sur = SDL_RenderReadPixels(ren, NULL);
    scc(sur, "Screenshot failed:");
    SDL_SaveBMP(sur, path);
    SDL_DestroySurface(sur);
}

// @param path Path of the IMG
SDL_Surface* LoadImg2Surf(const char* path)
{
    int w, h, channel;
    unsigned char* data = stbi_load(path, &w, &h, &channel, 0);
    scc(data, "Img Load failed:");

    if(w <= 0 || h <= 0){
        stbi_image_free(data);
        return nullptr;
    }

    SDL_PixelFormat format;
    switch(channel)
    {
        case 4:
            format = SDL_PIXELFORMAT_ARGB8888;
            break;
        case 1: case 3:
            format = SDL_PIXELFORMAT_RGB24;
            break;
        default:
            stbi_image_free(data);
            return nullptr;
    }

    SDL_Surface* sur = SDL_CreateSurface(w, h, format);
    scc(sur, "Img Load failed:");

    Uint8* dst = (Uint8* )sur->pixels;
    Uint8* src = data;
    const int pitch = sur->pitch;

    for(int i = 0; i < h; i++){
        switch(channel)
        {
            case 4:
                SDL_memcpy(dst, src, w * 4);
                src += w * 4;
                break;
            case 3:
                SDL_memcpy(dst, src, w * 3);
                src += w * 3;
                break;
            case 1:
                for(int j = 0; j < w; j++){
                    dst[3 * j + 0] = src[j];
                    dst[3 * j + 1] = src[j];
                    dst[3 * j + 2] = src[j];
                }
                src += w;
                break;
        }
        dst += pitch;
    }

    stbi_image_free(data);
    return sur;

}

/*
    @param surf SDL_Surface
    @param x X of the pixel
    @param y Y of the pixel
    @param r, g, b RGBA parameter
    @param a Coverage within [0.0, 1.0]
*/
void BlendPixel(SDL_Surface* surf, int x, int y, Uint8 r, Uint8 g, Uint8 b, float a)
{

    Uint32* pixels = (Uint32*)surf->pixels;
    int pitch = surf->pitch / 4;

    if(x < 0 || y < 0) return;
    if(x > surf->w || y > surf->h) return;
    if(a < 0.0f) return;
    if(a > 1.0f){
        pixels[y * pitch + x] = packRGBA(r, g, b, 255);
        return;
    }

    Uint32 dst = pixels[y * pitch + x];
    Uint8 dr, dg, db, da;
    unpackRGBA(dst, dr, dg, db, da);

    int r0 = (int)(  r * a + dr * (1.0f - a));
    int g0 = (int)(  g * a + dg * (1.0f - a));
    int b0 = (int)(  b * a + db * (1.0f - a));
    int a0 = (int)(255 * a + da * (1.0f - a));

    pixels[y * pitch + x] = packRGBA(
        (Uint8)std::clamp(r0, 0, 255),
        (Uint8)std::clamp(g0, 0, 255),
        (Uint8)std::clamp(b0, 0, 255),
        (Uint8)std::clamp(a0, 0, 255)
    );
    
}

// Circle with AA
void FillCircleAA(SDL_Surface* surf, float cx, float cy, float r, Uint8 cr, Uint8 cg, Uint8 cb) 
{
    int rCeil = (int)std::ceil(r);
    int yStart = (int)std::floor(cy - rCeil);
    int yEnd   = (int)std::ceil(cy + rCeil);

    float r2 = r * r;
    float rIn2  = (r - 0.5f) * (r - 0.5f);  
    float rOut2 = (r + 0.5f) * (r + 0.5f); 
    
    for (int py = yStart; py <= yEnd; py++) 
    {
        float dy = (float)py - cy;
        float dy2 = dy * dy;
    
        if (dy2 > rOut2) continue;
        
        float dxMax = std::sqrt(std::max(0.0f, r2 - dy2));
        float dxIn  = std::sqrt(std::max(0.0f, rIn2 - dy2));
        float dxOut = std::sqrt(std::max(0.0f, rOut2 - dy2));
        
        // @param xLeft, xRight X range for current row
        int xLeft  = (int)std::floor(cx - dxOut);
        int xRight = (int)std::ceil(cx + dxOut);
        
        for (int px = xLeft; px <= xRight; px++) 
        {
            float dx = (float)px - cx;
            float d2 = dx * dx + dy2;
            
            float alpha;
            if (d2 <= rIn2) alpha = 1.0f; 
            else if (d2 >= rOut2) alpha = 0.0f; 
            else alpha = std::clamp(1.0f - (std::sqrt(d2) - (r - 0.5f)), 0.0f, 1.0f);
           
            BlendPixel(surf, px, py, cr, cg, cb, alpha);
        }
    }
}
