#include "android_audio_backend_oboe.h"

#include "audio_internal.h"

#include <oboe/Oboe.h>

#include <atomic>
#include <memory>
#include <vector>

namespace {

class Backend final : public oboe::AudioStreamCallback {
public:
    Backend(int sampleRate, int framesPerBurst)
        : sampleRate_(sampleRate), framesPerBurst_(framesPerBurst) {}

    bool start() {
        oboe::AudioStreamBuilder builder;
        builder.setDirection(oboe::Direction::Output);
        builder.setPerformanceMode(oboe::PerformanceMode::LowLatency);
        builder.setSharingMode(oboe::SharingMode::Exclusive);
        builder.setFormat(oboe::AudioFormat::Float);
        builder.setChannelCount(2);
        builder.setSampleRate(sampleRate_);
        if (framesPerBurst_ > 0) builder.setFramesPerCallback(framesPerBurst_);
        builder.setCallback(this);

        oboe::Result r = builder.openStream(stream_);
        if (r != oboe::Result::OK || !stream_) return false;

        // Update engine device format to match actual stream.
        g_audio.sampleRate = stream_->getSampleRate();
        g_audio.framesPerBurst = stream_->getFramesPerBurst();

        r = stream_->requestStart();
        return r == oboe::Result::OK;
    }

    void stop() {
        if (stream_) {
            stream_->requestStop();
            stream_->close();
            stream_.reset();
        }
    }

    oboe::DataCallbackResult
    onAudioReady(oboe::AudioStream* /*audioStream*/, void* audioData, int32_t numFrames) override {
        // Render interleaved stereo float32.
        float* out = static_cast<float*>(audioData);
        AudioEngine_Render(out, numFrames);
        return oboe::DataCallbackResult::Continue;
    }

private:
    int sampleRate_ = 48000;
    int framesPerBurst_ = 192;
    std::shared_ptr<oboe::AudioStream> stream_;
};

} // namespace

extern "C" {

void* AndroidAudioBackendOboe_Create(int sampleRate, int framesPerBurst) {
    try {
        return new Backend(sampleRate, framesPerBurst);
    } catch (...) {
        return nullptr;
    }
}

void AndroidAudioBackendOboe_Destroy(void* backend) {
    auto* b = static_cast<Backend*>(backend);
    delete b;
}

bool AndroidAudioBackendOboe_Start(void* backend) {
    auto* b = static_cast<Backend*>(backend);
    if (!b) return false;
    return b->start();
}

void AndroidAudioBackendOboe_Stop(void* backend) {
    auto* b = static_cast<Backend*>(backend);
    if (!b) return;
    b->stop();
}

} // extern "C"

