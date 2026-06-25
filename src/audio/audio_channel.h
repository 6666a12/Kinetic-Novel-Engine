#pragma once
#include <SDL.h>
#include <thread>
#include <atomic>
#include <string>
#include <vector>
#include "graphics/fader.h"

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libswresample/swresample.h>
}

class AudioChannel
{
public:
    static constexpr int OUTPUT_RATE = 44800;
    static constexpr int OUTPUT_CHANNELS = 2;
    static constexpr int OUTPUT_BYTES_PER_SAMPLE = sizeof(int16_t);
    static constexpr int LOW_WATER_BYTES = OUTPUT_RATE * OUTPUT_CHANNELS * OUTPUT_BYTES_PER_SAMPLE;
    static constexpr int HIGH_WATER_BYTES = LOW_WATER_BYTES * 2;

    AudioChannel() = default;
    ~AudioChannel() { Stop(0.0); }

    bool Play(const char* path, SDL_AudioDeviceID device, bool loop = false)
    {
        Stop(0.0);
        if (!path || !*path || !device) return false;

        SDL_AudioSpec spec = { SDL_AUDIO_S16LE, OUTPUT_CHANNELS, OUTPUT_RATE };
        stream = SDL_CreateAudioStream(&spec, &spec);
        if (!stream) return false;

        if (SDL_BindAudioStream(device, stream) < 0)
        {
            SDL_DestroyAudioStream(stream);
            stream = nullptr;
            return false;
        }

        looping = loop;
        stopRequested = false;
        playing = true;
        finished = false;

        decodeThread = std::thread(&AudioChannel::DecodeLoop, this, std::string(path));
        return true;
    }

    void Stop(double fadeOutSec = 0.0)
    {
        if (fadeOutSec > 0.0 && playing)
        {
            gainFader.fadeOut(fadeOutSec);
            pendingStop = true;
            return;
        }

        pendingStop = false;
        stopRequested = true;
        playing = false;

        if (decodeThread.joinable())
            decodeThread.join();

        if (stream)
        {
            SDL_DestroyAudioStream(stream);
            stream = nullptr;
        }

        gainFader = Fader{1.0};
        finished = false;
    }

    void SetGain(float gain)
    {
        baseGain = std::clamp(gain, 0.0f, 1.0f);
        gainFader.fadeTo(static_cast<double>(baseGain), 0.0);
    }

    void FadeGainTo(float target, double durationSec)
    {
        baseGain = std::clamp(target, 0.0f, 1.0f);
        gainFader.fadeTo(static_cast<double>(baseGain), durationSec);
    }

    void Update(double dt)
    {
        gainFader.Update(dt);
        float gain = static_cast<float>(gainFader.Value());
        SDL_SetAudioStreamGain(stream, gain);

        if (pendingStop && !gainFader.isActive())
            Stop(0.0);
    }

    bool IsPlaying() const { return playing || !finished; }
    bool HasFinished() const { return finished && !playing; }
    float GetGain() const { return static_cast<float>(gainFader.Value()); }
    SDL_AudioStream* GetStream() const { return stream; }

private:
    std::atomic<bool> playing{false};
    std::atomic<bool> looping{false};
    std::atomic<bool> stopRequested{false};
    std::atomic<bool> finished{false};
    bool pendingStop = false;

    SDL_AudioStream* stream = nullptr;
    Fader gainFader{1.0};
    float baseGain = 1.0f;
    std::thread decodeThread;

    void DecodeLoop(const std::string& path)
    {
        AVFormatContext* fmtCtx = nullptr;
        if (avformat_open_input(&fmtCtx, path.c_str(), nullptr, nullptr) < 0)
            return;

        if (avformat_find_stream_info(fmtCtx, nullptr) < 0)
        {
            avformat_close_input(&fmtCtx);
            return;
        }

        int streamIdx = av_find_best_stream(fmtCtx, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
        if (streamIdx < 0)
        {
            avformat_close_input(&fmtCtx);
            return;
        }

        AVStream* avStream = fmtCtx->streams[streamIdx];
        const AVCodec* decoder = avcodec_find_decoder(avStream->codecpar->codec_id);
        if (!decoder)
        {
            avformat_close_input(&fmtCtx);
            return;
        }

        AVCodecContext* codecCtx = avcodec_alloc_context3(decoder);
        if (!codecCtx ||
            avcodec_parameters_to_context(codecCtx, avStream->codecpar) < 0 ||
            avcodec_open2(codecCtx, decoder, nullptr) < 0)
        {
            if (codecCtx) avcodec_free_context(&codecCtx);
            avformat_close_input(&fmtCtx);
            return;
        }

        AVChannelLayout outLayout = AV_CHANNEL_LAYOUT_STEREO;
        SwrContext* swr = nullptr;
        int srRet = swr_alloc_set_opts2(&swr,
            &outLayout, AV_SAMPLE_FMT_S16, OUTPUT_RATE,
            &codecCtx->ch_layout, codecCtx->sample_fmt, codecCtx->sample_rate,
            0, nullptr);
        if (srRet < 0 || !swr)
        {
            if (swr) swr_free(&swr);
            avcodec_free_context(&codecCtx);
            avformat_close_input(&fmtCtx);
            return;
        }
        swr_init(swr);

        AVPacket* packet = av_packet_alloc();
        AVFrame* frame = av_frame_alloc();
        std::vector<uint8_t> buf;

        while (!stopRequested)
        {
            if (SDL_GetAudioStreamQueued(stream) > HIGH_WATER_BYTES)
            {
                SDL_Delay(5);
                continue;
            }

            int ret = av_read_frame(fmtCtx, packet);
            if (ret < 0)
            {
                if (looping)
                {
                    av_seek_frame(fmtCtx, streamIdx, 0, AVSEEK_FLAG_BACKWARD);
                    avcodec_flush_buffers(codecCtx);
                    continue;
                }
                break;
            }

            if (packet->stream_index != streamIdx)
            {
                av_packet_unref(packet);
                continue;
            }

            ret = avcodec_send_packet(codecCtx, packet);
            av_packet_unref(packet);

            if (ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF)
                continue;

            while (true)
            {
                ret = avcodec_receive_frame(codecCtx, frame);
                if (ret < 0) break;

                int outSamples = swr_get_out_samples(swr, frame->nb_samples);
                if (outSamples <= 0) { av_frame_unref(frame); continue; }

                int bufBytes = (outSamples + 512) * OUTPUT_CHANNELS * OUTPUT_BYTES_PER_SAMPLE;
                if (buf.size() < static_cast<size_t>(bufBytes))
                    buf.resize(bufBytes);

                uint8_t* out = buf.data();
                int converted = swr_convert(swr, &out, outSamples + 512,
                                            (const uint8_t**)frame->extended_data,
                                            frame->nb_samples);
                if (converted > 0)
                    SDL_PutAudioStreamData(stream, buf.data(),
                                           converted * OUTPUT_CHANNELS * OUTPUT_BYTES_PER_SAMPLE);

                av_frame_unref(frame);
            }
        }

        av_frame_free(&frame);
        av_packet_free(&packet);
        swr_free(&swr);
        avcodec_free_context(&codecCtx);
        avformat_close_input(&fmtCtx);

        while (!stopRequested && SDL_GetAudioStreamQueued(stream) > 0)
            SDL_Delay(10);

        finished = true;
        playing = false;
    }
};
