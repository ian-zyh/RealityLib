#include "audio_internal.h"
#include "audio_mixer.h"
#include "audio_spatializer_simple.h"

#include <math.h>
#include <string.h>

static float clampf(float x, float a, float b) {
    return (x < a) ? a : (x > b) ? b : x;
}

static float lerpf(float a, float b, float t) {
    return a + (b - a) * t;
}

static Vector3 emitterPosForVoice(const AudioVoice* v) {
    if (v->emitter != AUDIO_INVALID_HANDLE) {
        uint32_t ei = v->emitter - 1u;
        if (ei < AUDIO_MAX_EMITTERS && atomic_load_explicit(&g_audio.emitters[ei].used, memory_order_acquire)) {
            return g_audio.emitters[ei].pos;
        }
    }
    return v->fixedPos;
}

static void mixVoiceToStereo(AudioVoice* v, float* outLR, int32_t frameCount) {
    uint32_t ci = v->clip - 1u;
    if (ci >= AUDIO_MAX_CLIPS) return;
    AudioClip* clip = &g_audio.clips[ci];
    if (!atomic_load_explicit(&clip->used, memory_order_acquire)) return;
    if (!clip->pcm.samples || clip->pcm.frameCount == 0) return;

    // Spatial gains (simple)
    Vector3 srcPos = emitterPosForVoice(v);
    float minDist = 0.5f;
    float maxDist = 25.0f;
    float rolloff = 1.0f;

    if (v->emitter != AUDIO_INVALID_HANDLE) {
        uint32_t ei = v->emitter - 1u;
        if (ei < AUDIO_MAX_EMITTERS && atomic_load_explicit(&g_audio.emitters[ei].used, memory_order_acquire)) {
            AudioEmitter* e = &g_audio.emitters[ei];
            minDist = e->minDist;
            maxDist = e->maxDist;
            rolloff = e->rolloff;
        }
    }

    AudioSpatialGains gains = AudioSpatializerSimple_ComputeGains(
        g_audio.listener.pos,
        g_audio.listener.ori,
        srcPos,
        v->gain * g_audio.masterVolume,
        minDist,
        maxDist,
        rolloff
    );

    // Resampling ratio: clipRate -> deviceRate, plus pitch.
    float clipRate = (float)clip->pcm.sampleRate;
    float devRate = (float)g_audio.sampleRate;
    float step = (clipRate / devRate) * v->pitch;
    if (step <= 0.00001f) step = 0.00001f;

    const float* s = clip->pcm.samples;
    uint32_t clipFrames = clip->pcm.frameCount;
    uint32_t ch = clip->pcm.channelCount;

    float fadeInFrames = (v->fadeInSeconds > 0.0f) ? (v->fadeInSeconds * devRate) : 0.0f;

    for (int32_t f = 0; f < frameCount; f++) {
        float t = v->cursorFrames;
        uint32_t i0 = (uint32_t)t;
        float frac = t - (float)i0;

        if (i0 + 1u >= clipFrames) {
            if (v->looping) {
                v->cursorFrames = 0.0f;
                t = 0.0f;
                i0 = 0;
                frac = 0.0f;
            } else {
                v->fadeOutRemaining = 0.0f;
                atomic_store_explicit(&v->used, false, memory_order_release);
                return;
            }
        }

        // Fetch mono or stereo samples with linear interpolation.
        float smL = 0.0f, smR = 0.0f;
        if (ch == 1) {
            float a = s[i0];
            float b = s[i0 + 1u];
            float m = lerpf(a, b, frac);
            smL = m;
            smR = m;
        } else {
            uint32_t base0 = i0 * 2u;
            uint32_t base1 = (i0 + 1u) * 2u;
            float aL = s[base0 + 0u], aR = s[base0 + 1u];
            float bL = s[base1 + 0u], bR = s[base1 + 1u];
            smL = lerpf(aL, bL, frac);
            smR = lerpf(aR, bR, frac);
        }

        float env = 1.0f;
        if (fadeInFrames > 0.0f) {
            float playedFrames = (t * (devRate / clipRate)); // approximate
            env *= clampf(playedFrames / fadeInFrames, 0.0f, 1.0f);
        }
        if (v->fadeOutRemaining > 0.0f && v->fadeOutSeconds > 0.0f) {
            float remain = v->fadeOutRemaining;
            env *= clampf(remain / (v->fadeOutSeconds * devRate), 0.0f, 1.0f);
            v->fadeOutRemaining -= 1.0f;
            if (v->fadeOutRemaining <= 0.0f) {
                atomic_store_explicit(&v->used, false, memory_order_release);
                return;
            }
        }

        outLR[f * 2 + 0] += smL * gains.left * env;
        outLR[f * 2 + 1] += smR * gains.right * env;

        v->cursorFrames += step;
    }
}

void AudioMixer_Mix(float* outInterleaved, int32_t frameCount) {
    memset(outInterleaved, 0, sizeof(float) * (size_t)frameCount * 2u);

    for (uint32_t i = 0; i < AUDIO_MAX_VOICES; i++) {
        AudioVoice* v = &g_audio.voices[i];
        if (!atomic_load_explicit(&v->used, memory_order_acquire)) continue;
        mixVoiceToStereo(v, outInterleaved, frameCount);
    }
}

