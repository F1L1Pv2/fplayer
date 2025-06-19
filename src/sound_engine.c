#include "3rdparty/miniaudio.h"
#include "sound_engine.h"
#include <string.h>
#include <malloc.h>
#include <stdatomic.h>
#include <stdio.h>

double soundTime = 0.0f;

typedef struct {
    AudioFrame* items;
    size_t count;
    atomic_int read_cur;
    atomic_int write_cur;
} AudioFrameFIFO;

void createAudioFrameFIFO(size_t count, AudioFrameFIFO* audioFrameFIFO) {
    audioFrameFIFO->items = calloc(sizeof(AudioFrame) * count,1);
    audioFrameFIFO->count = count;
    audioFrameFIFO->read_cur = 0;
    audioFrameFIFO->write_cur = 0;
}

AudioFrame* readAudioFrameFIFO(AudioFrameFIFO* audioFrameFIFO) {
    if (audioFrameFIFO->read_cur == audioFrameFIFO->write_cur) return NULL;
    AudioFrame* data = &audioFrameFIFO->items[audioFrameFIFO->read_cur];
    audioFrameFIFO->read_cur = (audioFrameFIFO->read_cur + 1) % audioFrameFIFO->count;
    return data;
}

bool canWriteAudioFrameFIFO(AudioFrameFIFO* audioFrameFIFO) {
    return !((audioFrameFIFO->write_cur + 1) % audioFrameFIFO->count == audioFrameFIFO->read_cur);
}

bool writeAudioFrameFIFO(AudioFrameFIFO* audioFrameFIFO, AudioFrame* item) {
    if (!canWriteAudioFrameFIFO(audioFrameFIFO)) return false;
    if(audioFrameFIFO->items[audioFrameFIFO->write_cur].data){
        free(audioFrameFIFO->items[audioFrameFIFO->write_cur].data);
        audioFrameFIFO->items[audioFrameFIFO->write_cur].data = NULL;
    }
    memcpy(&audioFrameFIFO->items[audioFrameFIFO->write_cur], item, sizeof(AudioFrame));
    audioFrameFIFO->write_cur = (audioFrameFIFO->write_cur + 1) % audioFrameFIFO->count;
    return true;
}

AudioFrameFIFO audioFrameFifo = {0};

bool soundEngineCanEnqueueFrame(){
    return canWriteAudioFrameFIFO(&audioFrameFifo);
}

static float engineVolume = 1.0f;

void soundEngineSetVolume(float volume){
    engineVolume = volume;
}

ma_device soundDevice;

bool soundEngineEnqueueFrame(AudioFrame* audioFrame){
    for(int i = 0; i < audioFrame->numberSamples*soundDevice.playback.channels; i++){
        ((float*)audioFrame->data)[i] *= engineVolume;
    }
    return writeAudioFrameFIFO(&audioFrameFifo, audioFrame);
}

static AudioFrame* currentFrame = NULL;
static size_t currentPos = 0;

void soundEngineResetQueue() {
    currentFrame = NULL;
    currentPos = 0;
    audioFrameFifo.read_cur = 0;
    audioFrameFifo.write_cur = 0;
}

extern atomic_bool playing;

void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
    float* output = (float*)pOutput;
    const ma_uint32 channels = pDevice->playback.channels;
    const size_t totalSamplesNeeded = frameCount * channels;
    size_t samplesCopied = 0;

    if(!playing) {
        memset(output, 0, totalSamplesNeeded);
        return;
    }
    if(playing) soundTime += (double)frameCount / (double)pDevice->sampleRate;

    while (samplesCopied < totalSamplesNeeded) {
        if (!currentFrame) {
            currentFrame = readAudioFrameFIFO(&audioFrameFifo);
            currentPos = 0;
            if (!currentFrame) break;
        }

        if(currentFrame && currentFrame->data){
            const size_t samplesAvailable = currentFrame->numberSamples * channels - currentPos;
            const size_t samplesNeeded = totalSamplesNeeded - samplesCopied;
            const size_t samplesToCopy = (samplesAvailable < samplesNeeded)
                ? samplesAvailable
                : samplesNeeded;

            float* frameData = (float*)currentFrame->data;
            memcpy(output + samplesCopied,
                frameData + currentPos,
                samplesToCopy * sizeof(float));
    
            samplesCopied += samplesToCopy;
            currentPos += samplesToCopy;
            if (currentPos >= currentFrame->numberSamples * channels) currentFrame = NULL;
        }
    }

    if (samplesCopied < totalSamplesNeeded) {
        memset(output + samplesCopied, 0, (totalSamplesNeeded - samplesCopied) * sizeof(float));
    }
}

bool initSoundEngine() {
    createAudioFrameFIFO(30,&audioFrameFifo);
    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.format = ma_format_f32;
    config.playback.channels = 2;
    config.sampleRate = 0;
    config.dataCallback = data_callback;
    config.pUserData = NULL;

    ma_result result = ma_device_init(NULL, &config, &soundDevice);
    if (result != MA_SUCCESS)
    {
        printf("Failed to initialize audio device. Error: %s\n", ma_result_description(result));
        return false;
    }

    size_t nameSize = 0;
    result = ma_device_get_name(&soundDevice, ma_device_type_playback, NULL, 0, &nameSize);
    if (nameSize != 0 && result == MA_SUCCESS) {
        char* name = malloc(nameSize + 1);
        if (ma_device_get_name(&soundDevice, ma_device_type_playback, name, nameSize + 1, NULL) == MA_SUCCESS) {
            printf("[Sound Engine] Using: %s audio device\n", name);
        }
        free(name);
    }

    ma_device_start(&soundDevice);
    return true;
}

uint32_t soundEngineGetSampleRate(){
    return soundDevice.sampleRate;
}

uint32_t soundEngineGetChannels(){
    return soundDevice.playback.channels;
}

double soundEngineGetTime(){
    return soundTime;
}

void soundEngineSetTime(double newTime){
    soundTime = newTime;
}