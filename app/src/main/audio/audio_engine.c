#include "audio_internal.h"
#include "audio_decoder.h"
#include "audio_mixer.h"
#include "android_audio_backend_oboe.h"

#include <stdlib.h>
#include <string.h>

static float clampf(float x, float a, float b) {
    return (x < a) ? a : (x > b) ? b : x;
}

static uint32_t allocHandleFromTable(atomic_bool* usedFlags, uint32_t capacity) {
    for (uint32_t i = 0; i < capacity; i++) {
        bool expected = false;
        if (atomic_compare_exchange_strong_explicit(&usedFlags[i], &expected, true, memory_order_acq_rel, memory_order_relaxed)) {
            return i + 1u;
        }
    }
    return AUDIO_INVALID_HANDLE;
}

uint32_t AudioEngine_AllocClip(void) {
    atomic_bool* flags = (atomic_bool*)&g_audio.clips[0].used;
    // flags are not contiguous if struct layout changes; allocate with explicit loop instead
    for (uint32_t i = 0; i < AUDIO_MAX_CLIPS; i++) {
        bool expected = false;
        if (atomic_compare_exchange_strong_explicit(&g_audio.clips[i].used, &expected, true, memory_order_acq_rel, memory_order_relaxed)) {
            return i + 1u;
        }
    }
    (void)flags;
    return AUDIO_INVALID_HANDLE;
}

uint32_t AudioEngine_AllocEmitter(void) {
    for (uint32_t i = 0; i < AUDIO_MAX_EMITTERS; i++) {
        bool expected = false;
        if (atomic_compare_exchange_strong_explicit(&g_audio.emitters[i].used, &expected, true, memory_order_acq_rel, memory_order_relaxed)) {
            return i + 1u;
        }
    }
    return AUDIO_INVALID_HANDLE;
}

uint32_t AudioEngine_AllocVoice(void) {
    for (uint32_t i = 0; i < AUDIO_MAX_VOICES; i++) {
        bool expected = false;
        if (atomic_compare_exchange_strong_explicit(&g_audio.voices[i].used, &expected, true, memory_order_acq_rel, memory_order_relaxed)) {
            return i + 1u;
        }
    }
    return AUDIO_INVALID_HANDLE;
}

void AudioEngine_ConsumeCommands(void) {
    AudioCommand cmd;
    while (AudioCmdDequeue(&cmd)) {
        switch (cmd.type) {
            case AUDIOCMD_SET_LISTENER:
                g_audio.listener.pos = cmd.u.listener.pos;
                g_audio.listener.ori = cmd.u.listener.ori;
                break;
            case AUDIOCMD_SET_MASTER_VOL:
                g_audio.masterVolume = clampf(cmd.u.master.volume, 0.0f, 2.0f);
                break;
            case AUDIOCMD_SET_SPATIALIZER:
                g_audio.spatializerMode = (AudioSpatializerMode)cmd.u.spatializer.mode;
                break;
            case AUDIOCMD_SET_ASSET_MGR:
                g_audio.assetManager = cmd.u.assetMgr.assetManager;
                break;
            case AUDIOCMD_DESTROY_EMITTER: {
                uint32_t h = cmd.a;
                if (h != AUDIO_INVALID_HANDLE) {
                    uint32_t idx = h - 1u;
                    if (idx < AUDIO_MAX_EMITTERS) {
                        atomic_store_explicit(&g_audio.emitters[idx].used, false, memory_order_release);
                    }
                }
            } break;
            case AUDIOCMD_SET_EMITTER_POSE: {
                uint32_t h = cmd.a;
                uint32_t idx = (h == 0) ? 0u : (h - 1u);
                if (h != AUDIO_INVALID_HANDLE && idx < AUDIO_MAX_EMITTERS &&
                    atomic_load_explicit(&g_audio.emitters[idx].used, memory_order_acquire)) {
                    g_audio.emitters[idx].pos = cmd.u.emitterPose.pos;
                    g_audio.emitters[idx].vel = cmd.u.emitterPose.vel;
                    g_audio.emitters[idx].ori = cmd.u.emitterPose.ori;
                }
            } break;
            case AUDIOCMD_SET_EMITTER_PARAMS: {
                uint32_t h = cmd.a;
                uint32_t idx = (h == 0) ? 0u : (h - 1u);
                if (h != AUDIO_INVALID_HANDLE && idx < AUDIO_MAX_EMITTERS &&
                    atomic_load_explicit(&g_audio.emitters[idx].used, memory_order_acquire)) {
                    AudioEmitter* e = &g_audio.emitters[idx];
                    e->gain = cmd.u.emitterParams.gain;
                    e->pitch = cmd.u.emitterParams.pitch;
                    e->minDist = cmd.u.emitterParams.minDist;
                    e->maxDist = cmd.u.emitterParams.maxDist;
                    e->rolloff = cmd.u.emitterParams.rolloff;
                    e->coneInnerDeg = cmd.u.emitterParams.coneInnerDeg;
                    e->coneOuterDeg = cmd.u.emitterParams.coneOuterDeg;
                    e->coneOuterGain = cmd.u.emitterParams.coneOuterGain;
                }
            } break;
            case AUDIOCMD_PLAY_ONE_SHOT: {
                uint32_t voice = cmd.a;
                uint32_t clip = cmd.u.oneShot.clip;
                if (voice != AUDIO_INVALID_HANDLE && voice - 1u < AUDIO_MAX_VOICES) {
                    AudioVoice* v = &g_audio.voices[voice - 1u];
                    v->looping = false;
                    v->clip = clip;
                    v->emitter = AUDIO_INVALID_HANDLE;
                    v->fixedPos = cmd.u.oneShot.pos;
                    v->gain = cmd.u.oneShot.gain;
                    v->pitch = 1.0f;
                    v->cursorFrames = 0.0f;
                    v->fadeInSeconds = 0.0f;
                    v->fadeOutSeconds = 0.0f;
                    v->fadeOutRemaining = 0.0f;
                }
            } break;
            case AUDIOCMD_PLAY_ON_EMITTER: {
                uint32_t voice = cmd.a;
                if (voice != AUDIO_INVALID_HANDLE && voice - 1u < AUDIO_MAX_VOICES) {
                    AudioVoice* v = &g_audio.voices[voice - 1u];
                    v->looping = cmd.u.playOnEmitter.params.loop;
                    v->clip = cmd.u.playOnEmitter.clip;
                    v->emitter = cmd.u.playOnEmitter.emitter;
                    v->fixedPos = (Vector3){0, 0, 0};
                    v->gain = cmd.u.playOnEmitter.params.gain;
                    v->pitch = (cmd.u.playOnEmitter.params.pitch > 0.0f) ? cmd.u.playOnEmitter.params.pitch : 1.0f;
                    v->cursorFrames = 0.0f;
                    v->fadeInSeconds = cmd.u.playOnEmitter.params.fadeInSeconds;
                    v->fadeOutSeconds = 0.0f;
                    v->fadeOutRemaining = 0.0f;
                }
            } break;
            case AUDIOCMD_STOP_VOICE: {
                uint32_t voice = cmd.u.stopVoice.voice;
                if (voice != AUDIO_INVALID_HANDLE && voice - 1u < AUDIO_MAX_VOICES) {
                    AudioVoice* v = &g_audio.voices[voice - 1u];
                    if (atomic_load_explicit(&v->used, memory_order_acquire)) {
                        float sec = cmd.u.stopVoice.fadeOutSeconds;
                        if (sec <= 0.0f) {
                            atomic_store_explicit(&v->used, false, memory_order_release);
                        } else {
                            v->fadeOutSeconds = sec;
                            v->fadeOutRemaining = sec * (float)g_audio.sampleRate;
                        }
                    }
                }
            } break;
            case AUDIOCMD_UNLOAD_CLIP: {
                uint32_t clip = cmd.u.unloadClip.clip;
                if (clip != AUDIO_INVALID_HANDLE && clip - 1u < AUDIO_MAX_CLIPS) {
                    AudioClip* c = &g_audio.clips[clip - 1u];
                    // Stop any voices referencing it.
                    for (uint32_t i = 0; i < AUDIO_MAX_VOICES; i++) {
                        AudioVoice* v = &g_audio.voices[i];
                        if (atomic_load_explicit(&v->used, memory_order_acquire) && v->clip == clip) {
                            atomic_store_explicit(&v->used, false, memory_order_release);
                        }
                    }
                    void* owned = c->ownedSamples;
                    c->ownedSamples = NULL;
                    c->pcm.samples = NULL;
                    c->pcm.frameCount = 0;
                    atomic_store_explicit(&c->used, false, memory_order_release);

                    if (owned) {
                        uint32_t n = atomic_fetch_add_explicit(&g_audio.pendingFreeCount, 1u, memory_order_acq_rel);
                        if (n < (uint32_t)(sizeof(g_audio.pendingFree) / sizeof(g_audio.pendingFree[0]))) {
                            g_audio.pendingFree[n] = owned;
                        }
                    }
                }
            } break;
            default:
                break;
        }
    }
}

void AudioEngine_Render(float* outInterleaved, int32_t frameCount) {
    AudioEngine_ConsumeCommands();
    AudioMixer_Mix(outInterleaved, frameCount);
}

// =============================================================================
// Public API implementations (game thread)
// =============================================================================

bool InitAudioEngine(int sampleRate, int framesPerBurst) {
    if (g_audio.initialized) return true;

    memset(&g_audio, 0, sizeof(g_audio));
    g_audio.sampleRate = (sampleRate > 0) ? sampleRate : 48000;
    g_audio.framesPerBurst = (framesPerBurst > 0) ? framesPerBurst : 192;
    g_audio.masterVolume = 1.0f;
    g_audio.spatializerMode = AUDIO_SPATIALIZER_SIMPLE;
    g_audio.listener.pos = (Vector3){0, 0, 0};
    g_audio.listener.ori = (Quaternion){0, 0, 0, 1};

    g_audio.backend = AndroidAudioBackendOboe_Create(g_audio.sampleRate, g_audio.framesPerBurst);
    if (!g_audio.backend) return false;
    if (!AndroidAudioBackendOboe_Start(g_audio.backend)) {
        AndroidAudioBackendOboe_Destroy(g_audio.backend);
        g_audio.backend = NULL;
        return false;
    }

    g_audio.initialized = true;
    return true;
}

void ShutdownAudioEngine(void) {
    if (!g_audio.initialized) return;

    if (g_audio.backend) {
        AndroidAudioBackendOboe_Stop(g_audio.backend);
        AndroidAudioBackendOboe_Destroy(g_audio.backend);
        g_audio.backend = NULL;
    }

    // Free owned clip buffers
    for (uint32_t i = 0; i < AUDIO_MAX_CLIPS; i++) {
        AudioClip* c = &g_audio.clips[i];
        if (atomic_load_explicit(&c->used, memory_order_acquire) && c->ownedSamples) {
            free(c->ownedSamples);
            c->ownedSamples = NULL;
        }
    }
    // Free pending frees (if any)
    uint32_t n = atomic_exchange_explicit(&g_audio.pendingFreeCount, 0u, memory_order_acq_rel);
    for (uint32_t i = 0; i < n && i < (uint32_t)(sizeof(g_audio.pendingFree) / sizeof(g_audio.pendingFree[0])); i++) {
        free(g_audio.pendingFree[i]);
        g_audio.pendingFree[i] = NULL;
    }

    g_audio.initialized = false;
}

void UpdateAudioEngine(float dt) {
    (void)dt;
    // Drain deferred frees (never in audio callback).
    uint32_t n = atomic_exchange_explicit(&g_audio.pendingFreeCount, 0u, memory_order_acq_rel);
    for (uint32_t i = 0; i < n && i < (uint32_t)(sizeof(g_audio.pendingFree) / sizeof(g_audio.pendingFree[0])); i++) {
        free(g_audio.pendingFree[i]);
        g_audio.pendingFree[i] = NULL;
    }
}

void SetAudioAssetManager(void* assetManager) {
    // Needed immediately for LoadAudioClip (which runs on the game thread).
    g_audio.assetManager = assetManager;
    AudioCommand cmd = {0};
    cmd.type = AUDIOCMD_SET_ASSET_MGR;
    cmd.u.assetMgr.assetManager = assetManager;
    (void)AudioCmdEnqueue(&cmd);
}

void SetAudioListenerPose(Vector3 position, Quaternion orientation) {
    AudioCommand cmd = {0};
    cmd.type = AUDIOCMD_SET_LISTENER;
    cmd.u.listener.pos = position;
    cmd.u.listener.ori = orientation;
    (void)AudioCmdEnqueue(&cmd);
}

void SetAudioMasterVolume(float volume) {
    AudioCommand cmd = {0};
    cmd.type = AUDIOCMD_SET_MASTER_VOL;
    cmd.u.master.volume = volume;
    (void)AudioCmdEnqueue(&cmd);
}

void SetAudioSpatializer(AudioSpatializerMode mode) {
    AudioCommand cmd = {0};
    cmd.type = AUDIOCMD_SET_SPATIALIZER;
    cmd.u.spatializer.mode = (uint32_t)mode;
    (void)AudioCmdEnqueue(&cmd);
}

AudioClipHandle LoadAudioClip(const char* assetPath, AudioClipLoadFlags flags) {
    (void)flags;
    if (!g_audio.initialized) return AUDIO_INVALID_HANDLE;

    uint32_t h = AudioEngine_AllocClip();
    if (h == AUDIO_INVALID_HANDLE) return AUDIO_INVALID_HANDLE;
    AudioClip* c = &g_audio.clips[h - 1u];

    float* samples = NULL;
    uint32_t frames = 0, rate = 0, ch = 0;
    if (!AudioDecoder_LoadWavFromAssets(assetPath, &samples, &frames, &rate, &ch)) {
        atomic_store_explicit(&c->used, false, memory_order_release);
        return AUDIO_INVALID_HANDLE;
    }

    c->streaming = (flags & AUDIOCLIP_STREAM) != 0;
    c->ownedSamples = samples;
    c->pcm.samples = samples;
    c->pcm.frameCount = frames;
    c->pcm.channelCount = ch;
    c->pcm.sampleRate = rate;

    return (AudioClipHandle)h;
}

void UnloadAudioClip(AudioClipHandle clip) {
    if (clip == AUDIO_INVALID_HANDLE) return;
    AudioCommand cmd = {0};
    cmd.type = AUDIOCMD_UNLOAD_CLIP;
    cmd.u.unloadClip.clip = clip;
    (void)AudioCmdEnqueue(&cmd);
}

AudioEmitterHandle CreateAudioEmitter(void) {
    uint32_t h = AudioEngine_AllocEmitter();
    if (h == AUDIO_INVALID_HANDLE) return AUDIO_INVALID_HANDLE;

    AudioEmitter* e = &g_audio.emitters[h - 1u];
    e->pos = (Vector3){0, 0, 0};
    e->vel = (Vector3){0, 0, 0};
    e->ori = (Quaternion){0, 0, 0, 1};
    e->gain = 1.0f;
    e->pitch = 1.0f;
    e->minDist = 0.5f;
    e->maxDist = 25.0f;
    e->rolloff = 1.0f;
    e->coneInnerDeg = 360.0f;
    e->coneOuterDeg = 360.0f;
    e->coneOuterGain = 1.0f;

    return (AudioEmitterHandle)h;
}

void DestroyAudioEmitter(AudioEmitterHandle e) {
    if (e == AUDIO_INVALID_HANDLE) return;
    AudioCommand cmd = {0};
    cmd.type = AUDIOCMD_DESTROY_EMITTER;
    cmd.a = e;
    (void)AudioCmdEnqueue(&cmd);
}

void SetAudioEmitterPose(AudioEmitterHandle e, Vector3 pos, Vector3 vel, Quaternion ori) {
    if (e == AUDIO_INVALID_HANDLE) return;
    AudioCommand cmd = {0};
    cmd.type = AUDIOCMD_SET_EMITTER_POSE;
    cmd.a = e;
    cmd.u.emitterPose.pos = pos;
    cmd.u.emitterPose.vel = vel;
    cmd.u.emitterPose.ori = ori;
    (void)AudioCmdEnqueue(&cmd);
}

void SetAudioEmitterParams(
    AudioEmitterHandle e,
    float gain,
    float pitch,
    float minDist,
    float maxDist,
    float rolloff,
    float coneInnerDeg,
    float coneOuterDeg,
    float coneOuterGain
) {
    if (e == AUDIO_INVALID_HANDLE) return;
    AudioCommand cmd = {0};
    cmd.type = AUDIOCMD_SET_EMITTER_PARAMS;
    cmd.a = e;
    cmd.u.emitterParams.gain = gain;
    cmd.u.emitterParams.pitch = pitch;
    cmd.u.emitterParams.minDist = minDist;
    cmd.u.emitterParams.maxDist = maxDist;
    cmd.u.emitterParams.rolloff = rolloff;
    cmd.u.emitterParams.coneInnerDeg = coneInnerDeg;
    cmd.u.emitterParams.coneOuterDeg = coneOuterDeg;
    cmd.u.emitterParams.coneOuterGain = coneOuterGain;
    (void)AudioCmdEnqueue(&cmd);
}

AudioVoiceHandle PlayOneShot3D(AudioClipHandle clip, Vector3 pos, float gain) {
    if (clip == AUDIO_INVALID_HANDLE) return AUDIO_INVALID_HANDLE;
    uint32_t v = AudioEngine_AllocVoice();
    if (v == AUDIO_INVALID_HANDLE) return AUDIO_INVALID_HANDLE;

    AudioCommand cmd = {0};
    cmd.type = AUDIOCMD_PLAY_ONE_SHOT;
    cmd.a = v;
    cmd.u.oneShot.clip = clip;
    cmd.u.oneShot.pos = pos;
    cmd.u.oneShot.gain = gain;
    (void)AudioCmdEnqueue(&cmd);
    return (AudioVoiceHandle)v;
}

AudioVoiceHandle PlayOnEmitter(AudioEmitterHandle e, AudioClipHandle clip, AudioPlayParams params) {
    if (e == AUDIO_INVALID_HANDLE || clip == AUDIO_INVALID_HANDLE) return AUDIO_INVALID_HANDLE;
    uint32_t v = AudioEngine_AllocVoice();
    if (v == AUDIO_INVALID_HANDLE) return AUDIO_INVALID_HANDLE;

    AudioCommand cmd = {0};
    cmd.type = AUDIOCMD_PLAY_ON_EMITTER;
    cmd.a = v;
    cmd.u.playOnEmitter.emitter = e;
    cmd.u.playOnEmitter.clip = clip;
    cmd.u.playOnEmitter.params = params;
    (void)AudioCmdEnqueue(&cmd);
    return (AudioVoiceHandle)v;
}

void StopVoice(AudioVoiceHandle v, float fadeOutSeconds) {
    if (v == AUDIO_INVALID_HANDLE) return;
    AudioCommand cmd = {0};
    cmd.type = AUDIOCMD_STOP_VOICE;
    cmd.u.stopVoice.voice = v;
    cmd.u.stopVoice.fadeOutSeconds = fadeOutSeconds;
    (void)AudioCmdEnqueue(&cmd);
}

