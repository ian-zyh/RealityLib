 #ifndef REALITYLIB_AUDIO_INTERNAL_H
 #define REALITYLIB_AUDIO_INTERNAL_H
 
 #include <stdbool.h>
 #include <stdint.h>
 #include <stdatomic.h>
 
 #include "../realitylib_audio.h"
 
 #ifdef __cplusplus
 extern "C" {
 #endif
 
 #ifndef AUDIO_MAX_CLIPS
 #define AUDIO_MAX_CLIPS 128
 #endif
 
 #ifndef AUDIO_MAX_EMITTERS
 #define AUDIO_MAX_EMITTERS 256
 #endif
 
 #ifndef AUDIO_MAX_VOICES
 #define AUDIO_MAX_VOICES 128
 #endif
 
 #ifndef AUDIO_COMMAND_CAPACITY
 #define AUDIO_COMMAND_CAPACITY 1024
 #endif
 
 typedef struct AudioPcmBuffer {
     // Interleaved or mono PCM stored as float32 in [-1,1]
     const float* samples;
     uint32_t frameCount;
     uint32_t channelCount; // 1 or 2
     uint32_t sampleRate;
 } AudioPcmBuffer;
 
 typedef struct AudioClip {
     atomic_bool used;
     bool streaming;
     AudioPcmBuffer pcm; // v1: decode-to-pcm only
     void* ownedSamples; // malloc'd float* we own
 } AudioClip;
 
 typedef struct AudioEmitter {
     atomic_bool used;
     Vector3 pos;
     Vector3 vel;
     Quaternion ori;
     float gain;
     float pitch;
     float minDist;
     float maxDist;
     float rolloff;
     float coneInnerDeg;
     float coneOuterDeg;
     float coneOuterGain;
 } AudioEmitter;
 
 typedef struct AudioVoice {
     atomic_bool used;
     bool looping;
     AudioClipHandle clip;
     AudioEmitterHandle emitter; // 0 if one-shot with fixed pos
     Vector3 fixedPos;
     float gain;
     float pitch;
     float cursorFrames; // fractional for resampling
     float fadeInSeconds;
     float fadeOutSeconds;
     float fadeOutRemaining;
 } AudioVoice;
 
 typedef enum AudioCommandType {
     AUDIOCMD_NONE = 0,
     AUDIOCMD_SET_LISTENER,
     AUDIOCMD_SET_MASTER_VOL,
     AUDIOCMD_SET_SPATIALIZER,
     AUDIOCMD_SET_ASSET_MGR,
     AUDIOCMD_CREATE_EMITTER,
     AUDIOCMD_DESTROY_EMITTER,
     AUDIOCMD_SET_EMITTER_POSE,
     AUDIOCMD_SET_EMITTER_PARAMS,
     AUDIOCMD_PLAY_ONE_SHOT,
     AUDIOCMD_PLAY_ON_EMITTER,
     AUDIOCMD_STOP_VOICE,
     AUDIOCMD_UNLOAD_CLIP,
 } AudioCommandType;
 
 typedef struct AudioCommand {
     AudioCommandType type;
     uint32_t a;
     uint32_t b;
     union {
         struct { Vector3 pos; Quaternion ori; } listener;
         struct { float volume; } master;
         struct { uint32_t mode; } spatializer;
         struct { void* assetManager; } assetMgr;
         struct { Vector3 pos; Vector3 vel; Quaternion ori; } emitterPose;
         struct {
             float gain;
             float pitch;
             float minDist;
             float maxDist;
             float rolloff;
             float coneInnerDeg;
             float coneOuterDeg;
             float coneOuterGain;
         } emitterParams;
         struct { Vector3 pos; float gain; uint32_t clip; } oneShot;
         struct { uint32_t emitter; uint32_t clip; AudioPlayParams params; } playOnEmitter;
         struct { uint32_t voice; float fadeOutSeconds; } stopVoice;
         struct { uint32_t clip; } unloadClip;
     } u;
 } AudioCommand;
 
 typedef struct AudioCommandQueue {
     AudioCommand items[AUDIO_COMMAND_CAPACITY];
     atomic_uint writeIdx;
     atomic_uint readIdx;
 } AudioCommandQueue;
 
 typedef struct AudioListenerState {
     Vector3 pos;
     Quaternion ori;
 } AudioListenerState;
 
 typedef struct AudioEngineState {
     bool initialized;
     int sampleRate;
     int framesPerBurst;
     float masterVolume;
     AudioSpatializerMode spatializerMode;
 
     AudioListenerState listener;
     void* assetManager; // AAssetManager*
 
     AudioClip clips[AUDIO_MAX_CLIPS];
     AudioEmitter emitters[AUDIO_MAX_EMITTERS];
     AudioVoice voices[AUDIO_MAX_VOICES];
 
     AudioCommandQueue cmdq;
 
     // Pointers queued for freeing on the game thread (never free in audio callback).
     void* pendingFree[256];
     atomic_uint pendingFreeCount;
 
     // backend-owned
     void* backend;
 } AudioEngineState;
 
 extern AudioEngineState g_audio;
 
 bool AudioCmdEnqueue(const AudioCommand* cmd);
 bool AudioCmdDequeue(AudioCommand* outCmd);
 
 // Called on audio thread
 void AudioEngine_ConsumeCommands(void);
 uint32_t AudioEngine_AllocVoice(void);
 uint32_t AudioEngine_AllocEmitter(void);
 uint32_t AudioEngine_AllocClip(void);
 
 // Audio thread render entrypoint (called from backend callback).
 // `outInterleaved` is interleaved stereo float32 (2 channels) for now.
 void AudioEngine_Render(float* outInterleaved, int32_t frameCount);
 
 #ifdef __cplusplus
 } // extern "C"
 #endif
 
 #endif
