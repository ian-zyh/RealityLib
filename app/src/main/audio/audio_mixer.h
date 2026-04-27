#ifndef REALITYLIB_AUDIO_MIXER_H
#define REALITYLIB_AUDIO_MIXER_H

#include <stdint.h>

void AudioMixer_Mix(float* outInterleaved, int32_t frameCount);

#endif
