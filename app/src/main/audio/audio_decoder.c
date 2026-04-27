#include "audio_internal.h"
#include "audio_decoder.h"

#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <stdlib.h>
#include <string.h>

static uint32_t readU32LE(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint16_t readU16LE(const uint8_t* p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static bool wavDecodePcm16Mono(const uint8_t* data, size_t size, float** outSamples, uint32_t* outFrames, uint32_t* outSampleRate, uint32_t* outChannels) {
    if (size < 44) return false;
    if (memcmp(data, "RIFF", 4) != 0) return false;
    if (memcmp(data + 8, "WAVE", 4) != 0) return false;

    uint32_t fmtAudioFormat = 0;
    uint32_t fmtChannels = 0;
    uint32_t fmtSampleRate = 0;
    uint32_t fmtBitsPerSample = 0;
    const uint8_t* pcmData = NULL;
    uint32_t pcmSize = 0;

    size_t off = 12;
    while (off + 8 <= size) {
        const uint8_t* chunk = data + off;
        uint32_t chunkSize = readU32LE(chunk + 4);
        const uint8_t* chunkData = chunk + 8;
        if (off + 8 + chunkSize > size) break;

        if (memcmp(chunk, "fmt ", 4) == 0 && chunkSize >= 16) {
            fmtAudioFormat = readU16LE(chunkData + 0);
            fmtChannels = readU16LE(chunkData + 2);
            fmtSampleRate = readU32LE(chunkData + 4);
            fmtBitsPerSample = readU16LE(chunkData + 14);
        } else if (memcmp(chunk, "data", 4) == 0) {
            pcmData = chunkData;
            pcmSize = chunkSize;
        }

        off += 8 + chunkSize + (chunkSize & 1u); // word alignment
    }

    if (!pcmData || pcmSize == 0) return false;
    if (fmtAudioFormat != 1) return false; // PCM
    if (fmtBitsPerSample != 16) return false;
    if (fmtChannels != 1 && fmtChannels != 2) return false;
    if (fmtSampleRate == 0) return false;

    uint32_t sampleCount = pcmSize / 2u;
    if (sampleCount == 0) return false;
    uint32_t frames = sampleCount / fmtChannels;

    float* samples = (float*)malloc(sizeof(float) * (size_t)frames * fmtChannels);
    if (!samples) return false;

    const int16_t* in = (const int16_t*)pcmData;
    for (uint32_t i = 0; i < frames * fmtChannels; i++) {
        samples[i] = (float)in[i] / 32768.0f;
    }

    *outSamples = samples;
    *outFrames = frames;
    *outSampleRate = fmtSampleRate;
    *outChannels = fmtChannels;
    return true;
}

bool AudioDecoder_LoadWavFromAssets(const char* assetPath, float** outSamples, uint32_t* outFrames, uint32_t* outSampleRate, uint32_t* outChannels) {
    if (!assetPath || !outSamples || !outFrames || !outSampleRate || !outChannels) return false;
    if (!g_audio.assetManager) return false;

    AAssetManager* mgr = (AAssetManager*)g_audio.assetManager;
    AAsset* asset = AAssetManager_open(mgr, assetPath, AASSET_MODE_BUFFER);
    if (!asset) return false;

    const void* buf = AAsset_getBuffer(asset);
    const off_t len = AAsset_getLength(asset);
    bool ok = false;

    if (buf && len > 0) {
        ok = wavDecodePcm16Mono((const uint8_t*)buf, (size_t)len, outSamples, outFrames, outSampleRate, outChannels);
    } else {
        // Fallback: read into memory
        off_t assetLen = AAsset_getLength(asset);
        uint8_t* tmp = (uint8_t*)malloc((size_t)assetLen);
        if (tmp) {
            int64_t readTotal = 0;
            while (readTotal < assetLen) {
                int32_t n = AAsset_read(asset, tmp + readTotal, (size_t)(assetLen - readTotal));
                if (n <= 0) break;
                readTotal += n;
            }
            if (readTotal == assetLen) {
                ok = wavDecodePcm16Mono(tmp, (size_t)assetLen, outSamples, outFrames, outSampleRate, outChannels);
            }
            free(tmp);
        }
    }

    AAsset_close(asset);
    return ok;
}

