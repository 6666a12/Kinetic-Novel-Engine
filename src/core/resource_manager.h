#pragma once

#include <thread>
#include <functional>
#include <unordered_map>
#include <variant>
#include <cmath>
#include <SDL.h>
#include <SDL_ttf.h>
#include "core/RAII.h"
#include "graphics/api.h"

namespace std
{
    template<>
    struct hash<std::pair<std::string, int>>
    {
        std::size_t operator()(const std::pair<std::string, int>& cp) const noexcept { 
            return std::hash<std::string>{}(cp.first) ^ (std::hash<int>{}(cp.second) << 1); 
        }        
    };
};

namespace RM{

    class ResourceManager
    {
    protected:
        SDL_Renderer*     ren;
        SDL_AudioDeviceID devID;
       
        std::unordered_map<std::variant<std::string, int>, Texture_ptr> textures;
        std::unordered_map<std::pair<std::string, int>, Font_ptr>       fonts;
        std::unordered_map<std::string, SDL_Color>                      colors;
        std::unordered_map<int, char*>                                  audios;

    public:
        ResourceManager(SDL_Renderer* rend, SDL_AudioDeviceID id)
        {
            SetAudioID(id);
            SetRenderer(rend);
            textures.clear();
            fonts.clear();
            colors.clear();
        }

        ~ResourceManager() = default;

        void SetAudioID(SDL_AudioDeviceID id){ devID = id; }
        SDL_AudioDeviceID GetAudioID(){ return devID; }

        bool SetColor(std::string tag, SDL_Color c)
        { 
            if(c.a == 0 || (c.r == 0 && c.b == 0 && c.g == 0) || tag.empty()) return false;
            
            colors.emplace(tag, c);
            return true; 
        }

        SDL_Color GetColor(std::string tag, SDL_Color fallback = SDL_Color{255, 255, 255, 255})
        { 
            if(colors.find(tag) == colors.end()) return fallback;

            return colors[tag];
        }

        bool SetTexture(std::variant<std::string, int> tag, const char* path)
        {
            if(std::holds_alternative<std::string>(tag)){
                std::string str = std::get<std::string>(tag);
                if(str.empty()) return false;
            }

            SDL_Surface* sur = LoadImg2Surf(path);
            SDL_Texture* texture0 = SDL_CreateTextureFromSurface(ren, sur);
            if(!texture0) return false;

            Texture_ptr pt(texture0);
            textures.emplace(tag, std::move(pt));
            SDL_DestroySurface(sur);
            return true;
        }

        SDL_Texture* GetTexture(std::variant<std::string, int> tag)
        { 
            if(textures.find(tag) == textures.end()) return nullptr;

            return textures[tag].get();
        }
        
        bool SetFont(std::pair<std::string, int> tag, const char* path)
        {
            if(tag.first.empty()) return false;

            float size = static_cast<float>(tag.second);
            TTF_Font* fon = TTF_OpenFont(path, size);
            if(!fon) fon = TTF_OpenFont("C:/Windows/Fonts/msyh.ttc", size);
            if(!fon) fon = TTF_OpenFont("C:/Windows/Fonts/simhei.ttf", size);
            if(!fon) fon = TTF_OpenFont("C:/Windows/Fonts/arial.ttf", size);
            if(!fon) return false;

            Font_ptr font(fon);
            fonts.emplace(tag, std::move(font));
            return true;
        }

        TTF_Font* GetFont(std::pair<std::string, int> tag)
        {
            if(fonts.find(tag) == fonts.end()) return nullptr;

            return fonts[tag].get();
        }

        bool SetRenderer(SDL_Renderer* rend)
        {  
            if(rend) ren = rend;
            return rend != nullptr;
        }
        
        bool SetAudioPath(int index, char* path)
        {
            if(audios.find(index) != audios.end()) return false;

            audios[index] = path;
            return true;
        }

        char* GetAudioPath(int index)
        {
            if(audios.find(index) == audios.end()) return nullptr;
            return audios[index];
        }
    };


};