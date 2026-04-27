#include "audio_internal.h"

AudioEngineState g_audio = {0};

static inline uint32_t nextIndex(uint32_t idx) {
    return (idx + 1u) % AUDIO_COMMAND_CAPACITY;
}

bool AudioCmdEnqueue(const AudioCommand* cmd) {
    uint32_t w = atomic_load_explicit(&g_audio.cmdq.writeIdx, memory_order_relaxed);
    uint32_t r = atomic_load_explicit(&g_audio.cmdq.readIdx, memory_order_acquire);
    uint32_t n = nextIndex(w);
    if (n == r) {
        return false; // full
    }
    g_audio.cmdq.items[w] = *cmd;
    atomic_store_explicit(&g_audio.cmdq.writeIdx, n, memory_order_release);
    return true;
}

bool AudioCmdDequeue(AudioCommand* outCmd) {
    uint32_t r = atomic_load_explicit(&g_audio.cmdq.readIdx, memory_order_relaxed);
    uint32_t w = atomic_load_explicit(&g_audio.cmdq.writeIdx, memory_order_acquire);
    if (r == w) {
        return false; // empty
    }
    *outCmd = g_audio.cmdq.items[r];
    atomic_store_explicit(&g_audio.cmdq.readIdx, nextIndex(r), memory_order_release);
    return true;
}

