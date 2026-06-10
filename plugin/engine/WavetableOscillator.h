#pragma once
// JUCE-free. nextSample() is RT-safe (no allocation, no locks).
#include <algorithm>
#include "Wavetable.h"

namespace soundx::engine {

class WavetableOscillator {
public:
    explicit WavetableOscillator(const Wavetable& wavetable) : table_(&wavetable) {}

    void setSampleRate(double sampleRate) {
        sampleRate_ = sampleRate;
        updateIncrement();
    }
    void setFrequency(float hz) {
        frequency_ = hz;
        updateIncrement();
    }
    void setPosition(float position01) { position_ = std::clamp(position01, 0.0f, 1.0f); }
    void reset() { phase_ = 0.0f; }

    float nextSample() {
        const float out = table_->sample(phase_, position_);
        phase_ += increment_;
        if (phase_ >= 1.0f)
            phase_ -= 1.0f;
        return out;
    }

private:
    void updateIncrement() {
        increment_ = sampleRate_ > 0.0 ? float(frequency_ / sampleRate_) : 0.0f;
    }

    const Wavetable* table_;
    double sampleRate_ = 0.0;
    float frequency_ = 0.0f;
    float increment_ = 0.0f;
    float phase_ = 0.0f;
    float position_ = 0.0f;
};

} // namespace soundx::engine
