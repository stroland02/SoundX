#pragma once
// JUCE-free. nextSample() is RT-safe (no allocation, no locks).
#include <algorithm>
#include <cmath>
#include "Wavetable.h"

namespace soundx::engine {

// Call setSampleRate() before setFrequency(); the phase increment is derived
// from both and recomputed on each setter call.
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

    // Retarget to different (already-built) banks. Safe between blocks only.
    // B may be null: the oscillator then reads bank A exclusively.
    void setTables(const Wavetable* a, const Wavetable* b) {
        if (a != nullptr)
            table_ = a;
        tableB_ = b;
    }

    // Blend amount between bank A (0) and bank B (1); ignored while B is null.
    void setMorph(float morph01) { morph_ = std::clamp(morph01, 0.0f, 1.0f); }

    float nextSample() noexcept {
        float out = table_->sample(phase_, position_);
        if (tableB_ != nullptr && morph_ > 0.0f)
            out += (tableB_->sample(phase_, position_) - out) * morph_;
        phase_ += increment_;
        phase_ -= std::floor(phase_);
        return out;
    }

private:
    void updateIncrement() {
        increment_ = sampleRate_ > 0.0 ? float(frequency_ / sampleRate_) : 0.0f;
    }

    const Wavetable* table_;
    const Wavetable* tableB_ = nullptr;
    float morph_ = 0.0f;
    double sampleRate_ = 0.0;
    float frequency_ = 0.0f;
    float increment_ = 0.0f;
    float phase_ = 0.0f;
    float position_ = 0.0f;
};

} // namespace soundx::engine
