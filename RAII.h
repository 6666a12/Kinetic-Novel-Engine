/*
    RAII.h
    
    Resource RAII
*/
#pragma once
#include <SDL.h>
#include <SDL_ttf.h>
#include <memory>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}


struct FFmpeg_deleter
{
    void operator()(AVFormatContext* p) const { if (p) avformat_close_input(&p); }
    void operator()(AVCodecContext* p) const { if (p) avcodec_free_context(&p); }
    void operator()(AVFrame* p) const { if (p) av_frame_free(&p); }
    void operator()(SwsContext* p) const { if (p) sws_freeContext(p); }
    void operator()(SwrContext* p) const { if (p) swr_free(&p); }
    void operator()(AVPacket* p) const { if (p) av_packet_free(&p); }
};

using AVFormatContext_ptr = std::unique_ptr<AVFormatContext, FFmpeg_deleter>;
using AVCodecContext_ptr = std::unique_ptr<AVCodecContext, FFmpeg_deleter>;
using AVFrame_ptr = std::unique_ptr<AVFrame, FFmpeg_deleter>;
using SwsContext_ptr = std::unique_ptr<SwsContext, FFmpeg_deleter>;
using SwrContext_ptr = std::unique_ptr<SwrContext, FFmpeg_deleter>;
using AVPacket_ptr = std::unique_ptr<AVPacket, FFmpeg_deleter>;

struct SDL_deleter{
    void operator()(SDL_Texture* t) const { if(t) SDL_DestroyTexture(t); }
    void operator()(SDL_Window *w) const { if(w) SDL_DestroyWindow(w); }
    void operator()(SDL_Surface *s) const { if(s) SDL_DestroySurface(s); }
    void operator()(SDL_Renderer *r) const { if(r) SDL_DestroyRenderer(r); }
    void operator()(SDL_Mutex *m) const { if(m) SDL_DestroyMutex(m); }
    void operator()(TTF_Font *f) const { if(f) TTF_CloseFont(f); }
};

using Texture_ptr = std::unique_ptr<SDL_Texture, SDL_deleter>;
using Window_ptr = std::unique_ptr<SDL_Window, SDL_deleter>;
using Surface_ptr = std::unique_ptr<SDL_Surface, SDL_deleter>;
using Renderer_ptr = std::unique_ptr<SDL_Renderer, SDL_deleter>;
using Mutex_ptr = std::unique_ptr<SDL_Mutex, SDL_deleter>;
using Font_ptr = std::unique_ptr<TTF_Font, SDL_deleter>;