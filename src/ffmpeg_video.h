#ifndef FVFX_FFMPEG_VIDEO
#define FVFX_FFMPEG_VIDEO

#include <vulkan/vulkan.h>
#include <stdbool.h>
#include <stdint.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>

typedef struct{
    void *data;
    double frameTime;
    size_t width;
    size_t height;
} Frame;

typedef struct {
    AVFormatContext* formatContext;
    int videoStreamIndex;
    AVCodecContext* codecContext;
    AVFrame* frame;
    AVPacket* packet;
    struct SwsContext* swsContext;
    double duration;
    double frameRate;
} Video;

bool ffmpegVideoInit(const char* filename, Video* video);
void ffmpegVideoUninit(Video* video);
bool ffmpegVideoGetFrame(Video* video, Frame* frame);
bool ffmpegVideoSeek(Video* video, Frame* frame, double time_seconds);

#endif