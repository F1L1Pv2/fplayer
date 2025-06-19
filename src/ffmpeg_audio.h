#ifndef FVFX_FFMPEG_AUDIO
#define FVFX_FFMPEG_AUDIO

#include <vulkan/vulkan.h>
#include <stdbool.h>
#include <stdint.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>

typedef struct {
    AVFormatContext* formatContext;
    int audioStreamIndex;
    AVCodecContext* codecContext;
    AVFrame* frame;
    AVPacket* packet;
    struct SwrContext* swrContext;
    double duration;
} Audio;

bool ffmpegAudioInit(const char* filename, Audio* audio);
void ffmpegAudioUninit(Audio* audio);
bool ffmpegAudioSeek(Audio* audio, double time_seconds);

#endif