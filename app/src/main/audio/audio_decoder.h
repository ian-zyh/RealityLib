#ifndef REALITYLIB_AUDIO_DECODER_H
#define REALITYLIB_AUDIO_DECODER_H

#include <stdbool.h>
#include <stdint.h>

// Load a WAV PCM16 clip from Android assets into float PCM.
bool AudioDecoder_LoadWavFromAssets(
    const char* assetPath,
    float** outSamples,
    uint32_t* outFrames,
    uint32_t* outSampleRate,
    uint32_t* outChannels
);

#endif
