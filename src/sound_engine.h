#ifndef FVFX_SOUND_ENGINE

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    void* data;
    size_t numberSamples;
} AudioFrame;

bool initSoundEngine();
uint32_t soundEngineGetSampleRate();
uint32_t soundEngineGetChannels();
bool soundEngineCanEnqueueFrame();
bool soundEngineEnqueueFrame(AudioFrame* audioFrame);
void soundEngineResetQueue();
double soundEngineGetTime();
void soundEngineSetTime(double newTime);
void soundEngineSetVolume(float volume);

#endif