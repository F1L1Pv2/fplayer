#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include <malloc.h>
#include "ffmpeg_audio.h"
#include "engine/vulkan_globals.h"
#include "engine/vulkan_helpers.h"
#include "engine/vulkan_images.h"
#include "sound_engine.h"
#include "engine/platform.h"
#include <threads.h>
#include <stdatomic.h>

static bool initializeAudioContext(Audio* audio, const char* filename);
static bool initializeAudioDecoder(Audio* audio);
bool ffmpegAudioGetFrame(Audio* audio, bool enqueue);

atomic_bool endWorker = false;
atomic_bool seeking = false;

extern atomic_bool playing;

int audioWorker(void* data){
    Audio* audio = (Audio*)data;
    while(true){
        if(endWorker) break;
        while (seeking || !playing) platform_sleep(1);
        ffmpegAudioGetFrame(audio, true);
    }

    return 0;
}

bool ffmpegAudioInit(const char* filename, Audio* audio) 
{
    memset(audio, 0, sizeof(Audio));
    
    if (!initializeAudioContext(audio, filename)) goto error;
    if (!initializeAudioDecoder(audio)) goto error;

    audio->duration = (double)audio->formatContext->streams[audio->audioStreamIndex]->duration * 
           av_q2d(audio->formatContext->streams[audio->audioStreamIndex]->time_base);

    thrd_t audioWorkerThread = {0};

    if(thrd_create(&audioWorkerThread,audioWorker,audio) != thrd_success) return false;

    thrd_detach(audioWorkerThread);

    return true;

error:
    ffmpegAudioUninit(audio);
    return false;
}

bool ffmpegAudioGetFrame(Audio* audio, bool enqueue) {
    av_frame_unref(audio->frame);
    int response;
    while (av_read_frame(audio->formatContext, audio->packet) >= 0) {
        if (audio->packet->stream_index != audio->audioStreamIndex) {
            av_packet_unref(audio->packet);
            continue;
        }

        response = avcodec_send_packet(audio->codecContext, audio->packet);
        if (response < 0) {
            av_packet_unref(audio->packet);
            continue;
        }

        response = avcodec_receive_frame(audio->codecContext, audio->frame);
        if (response == AVERROR(EAGAIN) || response == AVERROR_EOF) {
            av_packet_unref(audio->packet);
            continue;
        } 
        else if (response < 0) {
            av_packet_unref(audio->packet);
            return false;
        }

        if(enqueue){
            float* data = malloc(audio->frame->nb_samples * soundEngineGetChannels() * sizeof(float));
            uint8_t* out_data[1] = { (uint8_t*)data };
            int out_samples = swr_convert(audio->swrContext,
                out_data, audio->frame->nb_samples,
                (const uint8_t**)audio->frame->data, audio->frame->nb_samples);
        
            if (out_samples < 0) {
                free(data);
                continue;
            }
        
            AudioFrame audioFrame = {
                .data = data,
                .numberSamples = out_samples,
            };
    
            while(!soundEngineCanEnqueueFrame()){
                platform_sleep(1);
            }
    
            soundEngineEnqueueFrame(&audioFrame);
        }


        av_packet_unref(audio->packet);
        return true;
    }

    return false;
}

bool ffmpegAudioSeek(Audio* audio, double time_seconds) {
    seeking = true;
    int64_t target_pts = time_seconds / av_q2d(audio->formatContext->streams[audio->audioStreamIndex]->time_base);

    int ret = av_seek_frame(audio->formatContext, audio->audioStreamIndex, target_pts, AVSEEK_FLAG_BACKWARD);
    if (ret < 0) {
        printf("Seek failed: %s\n", av_err2str(ret));
        return false;
    }

    avcodec_flush_buffers(audio->codecContext);
 
    do {
        if (!ffmpegAudioGetFrame(audio, false)) break;
    } while((double)audio->frame->pts *av_q2d(audio->formatContext->streams[audio->audioStreamIndex]->time_base) < time_seconds);

    soundEngineResetQueue();

    seeking = false;

    return true;
}

void ffmpegAudioUninit(Audio* audio) {
    if (audio->codecContext) avcodec_free_context(&audio->codecContext);
    if (audio->frame) av_frame_free(&audio->frame);
    if (audio->packet) av_packet_free(&audio->packet);
    if (audio->formatContext) {
        avformat_close_input(&audio->formatContext);
        avformat_free_context(audio->formatContext);
    }
    
    memset(audio, 0, sizeof(Audio));
}

static bool initializeAudioContext(Audio* audio, const char* filename) {
    audio->formatContext = avformat_alloc_context();
    if (!audio->formatContext) return false;
    
    if (avformat_open_input(&audio->formatContext, filename, NULL, NULL) < 0) {
        avformat_free_context(audio->formatContext);
        return false;
    }
    
    if (avformat_find_stream_info(audio->formatContext, NULL) < 0) return false;
    
    return true;
}

static bool initializeAudioDecoder(Audio* audio) {
    audio->audioStreamIndex = -1;
    for (int i = 0; i < audio->formatContext->nb_streams; i++) {
        AVStream* stream = audio->formatContext->streams[i];
        if(stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO){
            audio->audioStreamIndex = i;
            AVCodecParameters* codecParameters = stream->codecpar;

            const AVCodec* codec = avcodec_find_decoder(codecParameters->codec_id);
            if (!codec) return false;
            
            audio->codecContext = avcodec_alloc_context3(codec);
            if (!audio->codecContext) return false;
            
            if (avcodec_parameters_to_context(audio->codecContext, codecParameters) < 0) return false;
            
            if (avcodec_open2(audio->codecContext, codec, NULL) < 0) return false;

            if(!initSoundEngine()) return false;

            int ret = swr_alloc_set_opts2(&audio->swrContext,
                soundEngineGetChannels() == 2 ? &(AVChannelLayout)AV_CHANNEL_LAYOUT_STEREO : &(AVChannelLayout)AV_CHANNEL_LAYOUT_MONO,
                AV_SAMPLE_FMT_FLT,
                soundEngineGetSampleRate(),
                &audio->codecContext->ch_layout,
                audio->codecContext->sample_fmt,
                codecParameters->sample_rate,
                0,
                NULL);

            if (!audio->swrContext || swr_init(audio->swrContext) < 0) {
                fprintf(stderr, "Failed to initialize resampler\n");
                return false;
            }

            break;
        }
    }
    
    if (audio->audioStreamIndex == -1) return false;
    
    audio->frame = av_frame_alloc();
    audio->packet = av_packet_alloc();
    return audio->frame && audio->packet;
}