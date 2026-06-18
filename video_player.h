#pragma once

#include "Iplayable.h"
#include "RAII.h"

#include <SDL3/SDL.h>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}
\


struct VideoFrame
{
    std::vector<uint8_t> pixels;
    double pts = 0.0;
};

class VideoPlayer : public Iplayable
{
public:
    VideoPlayer() = default;
    ~VideoPlayer() override { Close(); }

    bool Open(const char* filename, SDL_Renderer* renderer, SDL_AudioStream* stream)
    {
        Close();

        sourceFilename = filename ? filename : "";
        audioStream = stream;
        if (audioStream)
            SDL_ClearAudioStream(audioStream);

        if (!OpenVideoStream(renderer))
        {
            fprintf(stderr, "Failed to open video stream: %s\n", sourceFilename.c_str());
            Close();
            return false;
        }

        audioEnabled = audioStream && OpenAudioStream();
        if (audioStream && !audioEnabled)
            fprintf(stderr, "Audio stream disabled: failed to open audio stream\n");

        running = true;
        playing = true;
        videoFinished = false;
        audioFinished = !audioEnabled;
        playbackStartCounter = SDL_GetPerformanceCounter();
        playbackFrequency = SDL_GetPerformanceFrequency();
        lastQueuedVideoPts.store(0.0);

        if (audioEnabled)
            audioThread = std::thread(&VideoPlayer::AudioThreadFunc, this);
        videoThread = std::thread(&VideoPlayer::VideoThreadFunc, this);
        return true;
    }

    void Close() override
    {
        running = false;
        frameQueueCond.notify_all();

        if (audioThread.joinable())
            audioThread.join();
        if (videoThread.joinable())
            videoThread.join();

        playing = false;

        if (audioStream)
            SDL_ClearAudioStream(audioStream);

        if (videoTexture)
        {
            SDL_DestroyTexture(videoTexture);
            videoTexture = nullptr;
        }

        {
            std::lock_guard<std::mutex> lock(frameQueueMutex);
            videoFrameQueue.clear();
        }

        videoFmtCtx.reset();
        audioFmtCtx.reset();
        vCodecCtx.reset();
        aCodecCtx.reset();
        vFrame.reset();
        aFrame.reset();
        swsCtx.reset();
        swrCtx.reset();
        rgbBuffer.clear();
        audioBuffer.clear();

        sourceFilename.clear();
        audioStream = nullptr;
        videoStreamIdx = -1;
        audioStreamIdx = -1;
        videoWidth = 0;
        videoHeight = 0;
        frameDuration = 1.0 / 30.0;
        playbackStartCounter = 0;
        playbackFrequency = 1;
        nextVideoPts = 0.0;
        lastQueuedVideoPts.store(0.0);
        videoTimeBase = {1, 1000};
        audioEnabled = false;
        videoFinished = false;
        audioFinished = false;
    }

    bool Update(double dt) override
    {
        if (!playing)
            return false;

        const double clock = PlaybackClock(dt);
        std::vector<uint8_t> frameToUpload;
        bool updated = false;

        {
            std::lock_guard<std::mutex> lock(frameQueueMutex);

            while (!videoFrameQueue.empty() &&
                   videoFrameQueue.front().pts < clock - LateFrameKeepSeconds)
            {
                frameToUpload = std::move(videoFrameQueue.front().pixels);
                videoFrameQueue.pop_front();
                updated = true;
            }

            while (!videoFrameQueue.empty() &&
                   videoFrameQueue.front().pts <= clock + VideoSyncSlackSeconds)
            {
                frameToUpload = std::move(videoFrameQueue.front().pixels);
                videoFrameQueue.pop_front();
                updated = true;
            }
        }

        if (updated && !frameToUpload.empty())
            SDL_UpdateTexture(videoTexture, nullptr, frameToUpload.data(), videoWidth * 4);

        frameQueueCond.notify_one();

        if (videoFinished && videoFrameQueue.empty())
        {
            if (!audioEnabled || (audioFinished && SDL_GetAudioStreamQueued(audioStream) == 0))
            {
                playing = false;
                return false;
            }
        }

        return true;
    }

    void Render(SDL_Renderer* renderer, const SDL_FRect* dstRect = nullptr) override
    {
        if (!videoTexture)
            return;

        if (dstRect)
            SDL_RenderTexture(renderer, videoTexture, nullptr, dstRect);
        else
            SDL_RenderTexture(renderer, videoTexture, nullptr, nullptr);
    }

    bool IsPlaying() const override { return playing; }

private:
    static constexpr int AudioOutputRate = 44800;
    static constexpr int AudioOutputChannels = 2;
    static constexpr int AudioOutputBytesPerSample = sizeof(int16_t);
    static constexpr int AudioLowWaterBytes =
        AudioOutputRate * AudioOutputChannels * AudioOutputBytesPerSample;
    static constexpr int AudioHighWaterBytes = AudioLowWaterBytes * 2;
    static constexpr size_t MaxVideoFrameQueue = 12;
    static constexpr double MaxVideoLeadSeconds = 0.25;
    static constexpr double LateFrameKeepSeconds = 0.08;
    static constexpr double VideoSyncSlackSeconds = 0.025;

    std::string sourceFilename;

    AVFormatContext_ptr videoFmtCtx;
    AVFormatContext_ptr audioFmtCtx;
    AVCodecContext_ptr vCodecCtx;
    AVCodecContext_ptr aCodecCtx;
    AVFrame_ptr vFrame;
    AVFrame_ptr aFrame;
    SwsContext_ptr swsCtx;
    SwrContext_ptr swrCtx;

    SDL_Texture* videoTexture = nullptr;
    SDL_AudioStream* audioStream = nullptr;

    int videoStreamIdx = -1;
    int audioStreamIdx = -1;
    int videoWidth = 0;
    int videoHeight = 0;
    double frameDuration = 1.0 / 30.0;
    Uint64 playbackStartCounter = 0;
    Uint64 playbackFrequency = 1;
    double nextVideoPts = 0.0;
    std::atomic<double> lastQueuedVideoPts{0.0};
    AVRational videoTimeBase{1, 1000};

    std::vector<uint8_t> rgbBuffer;
    std::vector<uint8_t> audioBuffer;
    std::deque<VideoFrame> videoFrameQueue;
    std::mutex frameQueueMutex;
    std::condition_variable frameQueueCond;

    std::thread audioThread;
    std::thread videoThread;
    std::atomic<bool> running{false};
    std::atomic<bool> playing{false};
    std::atomic<bool> audioEnabled{false};
    std::atomic<bool> videoFinished{false};
    std::atomic<bool> audioFinished{false};

    static bool OpenFormat(const std::string& filename, AVFormatContext_ptr& outFmt)
    {
        AVFormatContext* raw = nullptr;
        if (avformat_open_input(&raw, filename.c_str(), nullptr, nullptr) < 0)
            return false;

        outFmt.reset(raw);
        if (avformat_find_stream_info(outFmt.get(), nullptr) < 0)
            return false;

        return true;
    }

    static bool OpenDecoder(AVFormatContext* fmt, AVMediaType type, int& streamIdx,
                            AVCodecContext_ptr& codecCtx)
    {
        streamIdx = av_find_best_stream(fmt, type, -1, -1, nullptr, 0);
        if (streamIdx < 0)
            return false;

        AVStream* stream = fmt->streams[streamIdx];
        const AVCodec* decoder = avcodec_find_decoder(stream->codecpar->codec_id);
        if (!decoder)
            return false;

        codecCtx.reset(avcodec_alloc_context3(decoder));
        if (!codecCtx)
            return false;

        if (avcodec_parameters_to_context(codecCtx.get(), stream->codecpar) < 0)
            return false;

        return avcodec_open2(codecCtx.get(), decoder, nullptr) >= 0;
    }

    bool OpenVideoStream(SDL_Renderer* renderer)
    {
        if (!OpenFormat(sourceFilename, videoFmtCtx))
            return false;

        if (!OpenDecoder(videoFmtCtx.get(), AVMEDIA_TYPE_VIDEO, videoStreamIdx, vCodecCtx))
            return false;

        AVStream* stream = videoFmtCtx->streams[videoStreamIdx];
        videoWidth = vCodecCtx->width;
        videoHeight = vCodecCtx->height;
        if (videoWidth <= 0 || videoHeight <= 0)
            return false;

        vFrame.reset(av_frame_alloc());
        if (!vFrame)
            return false;

        rgbBuffer.resize(static_cast<size_t>(videoWidth) * videoHeight * 4);
        swsCtx.reset(sws_getContext(videoWidth, videoHeight, vCodecCtx->pix_fmt,
                                    videoWidth, videoHeight, AV_PIX_FMT_RGBA,
                                    SWS_BILINEAR, nullptr, nullptr, nullptr));
        if (!swsCtx)
            return false;

        videoTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32,
                                         SDL_TEXTUREACCESS_STREAMING,
                                         videoWidth, videoHeight);
        if (!videoTexture)
            return false;

        SDL_SetTextureScaleMode(videoTexture, SDL_SCALEMODE_LINEAR);

        AVRational fr = stream->avg_frame_rate;
        if (fr.den == 0 || fr.num == 0)
            fr = stream->r_frame_rate;
        frameDuration = (fr.den > 0 && fr.num > 0) ? static_cast<double>(fr.den) / fr.num
                                                   : 1.0 / 30.0;
        videoTimeBase = stream->time_base;
        return true;
    }

    bool OpenAudioStream()
    {
        if (!OpenFormat(sourceFilename, audioFmtCtx))
            return false;

        if (!OpenDecoder(audioFmtCtx.get(), AVMEDIA_TYPE_AUDIO, audioStreamIdx, aCodecCtx))
            return false;

        AVChannelLayout outLayout = AV_CHANNEL_LAYOUT_STEREO;
        SwrContext* rawSwr = nullptr;
        int ret = swr_alloc_set_opts2(&rawSwr,
                                      &outLayout, AV_SAMPLE_FMT_S16, AudioOutputRate,
                                      &aCodecCtx->ch_layout, aCodecCtx->sample_fmt,
                                      aCodecCtx->sample_rate,
                                      0, nullptr);
        if (ret < 0 || !rawSwr || swr_init(rawSwr) < 0)
        {
            if (rawSwr)
                swr_free(&rawSwr);
            return false;
        }

        swrCtx.reset(rawSwr);
        aFrame.reset(av_frame_alloc());
        return aFrame != nullptr;
    }

    double PlaybackClock(double dt)
    {
        if (playbackStartCounter == 0 || playbackFrequency == 0)
            return dt;

        const Uint64 now = SDL_GetPerformanceCounter();
        return static_cast<double>(now - playbackStartCounter) /
               static_cast<double>(playbackFrequency);
    }

    bool AudioNeedsData() const
    {
        return audioEnabled && audioStream &&
               SDL_GetAudioStreamQueued(audioStream) < AudioLowWaterBytes;
    }

    void AudioThreadFunc()
    {
        AVPacket_ptr packet(av_packet_alloc());
        bool reachedEof = false;
        while (running)
        {
            if (SDL_GetAudioStreamQueued(audioStream) > AudioHighWaterBytes)
            {
                SDL_Delay(5);
                continue;
            }

            int ret = av_read_frame(audioFmtCtx.get(), packet.get());
            if (ret < 0)
            {
                reachedEof = true;
                break;
            }

            if (packet->stream_index == audioStreamIdx)
                DecodeAudioPacket(packet.get());

            av_packet_unref(packet.get());
        }

        if (reachedEof && running)
            DrainAudioDecoder();
        audioFinished = true;
    }

    void VideoThreadFunc()
    {
        AVPacket_ptr packet(av_packet_alloc());
        bool reachedEof = false;
        while (running)
        {
            ThrottleVideoDecode();

            int ret = av_read_frame(videoFmtCtx.get(), packet.get());
            if (ret < 0)
            {
                reachedEof = true;
                break;
            }

            if (packet->stream_index == videoStreamIdx)
                DecodeVideoPacket(packet.get());

            av_packet_unref(packet.get());
        }

        if (reachedEof && running)
            DrainVideoDecoder();
        videoFinished = true;
        frameQueueCond.notify_all();
    }

    void ThrottleVideoDecode()
    {
        while (running)
        {
            const double lead = lastQueuedVideoPts.load() - PlaybackClock(0.0);
            if (lead <= MaxVideoLeadSeconds)
                return;

            std::unique_lock<std::mutex> lock(frameQueueMutex);
            frameQueueCond.wait_for(lock, std::chrono::milliseconds(5), [this] {
                return !running ||
                       (lastQueuedVideoPts.load() - PlaybackClock(0.0)) <= MaxVideoLeadSeconds;
            });
        }
    }

    void DecodeVideoPacket(AVPacket* packet)
    {
        int ret = avcodec_send_packet(vCodecCtx.get(), packet);
        if (ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF)
            return;

        while (ret == AVERROR(EAGAIN))
        {
            if (!ReceiveAndQueueVideoFrame())
                return;
            ret = avcodec_send_packet(vCodecCtx.get(), packet);
        }

        while (ReceiveAndQueueVideoFrame()) {}
    }

    void DrainVideoDecoder()
    {
        avcodec_send_packet(vCodecCtx.get(), nullptr);
        while (ReceiveAndQueueVideoFrame()) {}
    }

    bool ReceiveAndQueueVideoFrame()
    {
        int ret = avcodec_receive_frame(vCodecCtx.get(), vFrame.get());
        if (ret < 0)
            return false;

        uint8_t* dstData[1] = {rgbBuffer.data()};
        int dstLinesize[1] = {videoWidth * 4};
        sws_scale(swsCtx.get(), vFrame->data, vFrame->linesize,
                  0, videoHeight, dstData, dstLinesize);

        VideoFrame frame;
        frame.pixels.assign(rgbBuffer.begin(), rgbBuffer.end());

        const int64_t pts = vFrame->best_effort_timestamp;
        if (pts != AV_NOPTS_VALUE)
        {
            frame.pts = pts * av_q2d(videoTimeBase);
            nextVideoPts = frame.pts + frameDuration;
        }
        else
        {
            frame.pts = nextVideoPts;
            nextVideoPts += frameDuration;
        }

        QueueVideoFrame(std::move(frame));
        av_frame_unref(vFrame.get());
        return true;
    }

    void QueueVideoFrame(VideoFrame frame)
    {
        std::lock_guard<std::mutex> lock(frameQueueMutex);

        const double clock = PlaybackClock(0.0);
        while (videoFrameQueue.size() > 1 &&
               videoFrameQueue[1].pts < clock - LateFrameKeepSeconds)
        {
            videoFrameQueue.pop_front();
        }

        while (videoFrameQueue.size() >= MaxVideoFrameQueue)
            videoFrameQueue.pop_front();

        lastQueuedVideoPts.store(frame.pts);
        videoFrameQueue.push_back(std::move(frame));
        frameQueueCond.notify_one();
    }

    void DecodeAudioPacket(AVPacket* packet)
    {
        int ret = avcodec_send_packet(aCodecCtx.get(), packet);
        if (ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF)
            return;

        while (ret == AVERROR(EAGAIN))
        {
            if (!ReceiveAndQueueAudioFrame())
                return;
            ret = avcodec_send_packet(aCodecCtx.get(), packet);
        }

        while (ReceiveAndQueueAudioFrame()) {}
    }

    void DrainAudioDecoder()
    {
        avcodec_send_packet(aCodecCtx.get(), nullptr);
        while (ReceiveAndQueueAudioFrame()) {}
    }

    bool ReceiveAndQueueAudioFrame()
    {
        int ret = avcodec_receive_frame(aCodecCtx.get(), aFrame.get());
        if (ret < 0)
            return false;

        const int outSamples = swr_get_out_samples(swrCtx.get(), aFrame->nb_samples);
        if (outSamples <= 0)
        {
            av_frame_unref(aFrame.get());
            return false;
        }

        const size_t bufSize = static_cast<size_t>(outSamples + 512) *
                               AudioOutputChannels * AudioOutputBytesPerSample;
        if (audioBuffer.size() < bufSize)
            audioBuffer.resize(bufSize);

        uint8_t* outData = audioBuffer.data();
        const int converted = swr_convert(swrCtx.get(), &outData,
                                          outSamples + 512,
                                          (const uint8_t**)aFrame->extended_data,
                                          aFrame->nb_samples);
        if (converted > 0)
        {
            const int outBytes = converted * AudioOutputChannels * AudioOutputBytesPerSample;
            SDL_PutAudioStreamData(audioStream, outData, outBytes);
        }

        av_frame_unref(aFrame.get());
        return true;
    }
};
