#ifndef REALITYLIB_ANDROID_AUDIO_BACKEND_OBOE_H
#define REALITYLIB_ANDROID_AUDIO_BACKEND_OBOE_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Opaque backend object owned by the engine.
void* AndroidAudioBackendOboe_Create(int sampleRate, int framesPerBurst);
void AndroidAudioBackendOboe_Destroy(void* backend);

// Start/stop stream.
bool AndroidAudioBackendOboe_Start(void* backend);
void AndroidAudioBackendOboe_Stop(void* backend);

#ifdef __cplusplus
}
#endif

#endif
