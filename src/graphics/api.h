#pragma once
#define STB_IMAGE_IMPLEMENTATION

#include "core/stb_image.h"
#include "core/scc.h"
#include <algorithm>
#include <SDL.h>
#include <cmath>

void screenshot(SDL_Renderer*, char*);
SDL_Surface* LoadImg2Surf(const char* path);
void unpackRGBA(Uint32 pixel, Uint8 &r, Uint8 &g, Uint8 &b, Uint8 &a);
Uint32 packRGBA(Uint8 r, Uint8 g, Uint8 b, Uint8 a);
void BlendPixel(SDL_Surface* surf, int x, int y, Uint8 r, Uint8 g, Uint8 b, float a);
void FillCircleAA(SDL_Surface* surf, float cx, float cy, float r, Uint8 cr, Uint8 cg, Uint8 cb);

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