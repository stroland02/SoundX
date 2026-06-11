#pragma once
// JUCE-free control-rate LFO. advance() is RT-safe; call once per audio block
// (or any sample count) — it advances the phase and returns the new value.
#include <algorithm>
#include <cmath>
#include <numbers>
#include "Rng.h"

namespace soundx::engine {

class Lfo {
public:
    enum class Shape { sine = 0, triangle, saw, square, sampleHold };

    void setSampleRate(double sampleRate) { sampleRate_ = sampleRate; }
    void setRate(float hz) { rateHz_ = std::clamp(hz, 0.01f, 20.0f); }
    void setShape(Shape shape) { shape_ = shape; }

    // Advances by numSamples and returns the LFO value in [-1, 1].
    float advance(int numSamples) noexcept {
        if (sampleRate_ <= 0.0)
            return 0.0f;
        phase_ += double(rateHz_) * double(numSamples) / sampleRate_;
        if (phase_ >= 1.0) {
            phase_ -= std::floor(phase_);
            heldValue_ = rng_.next01() * 2.0f - 1.0f; // refresh S&H on wrap
        }
        const float p = float(phase_);
        switch (shape_) {
        case Shape::sine:
            return std::sin(2.0f * float(std::numbers::pi) * p);
        case Shape::triangle:
            // starts at -1, peaks +1 at phase 0.5
            return p < 0.5f ? -1.0f + 4.0f * p : 3.0f - 4.0f * p;
        case Shape::saw:
            return -1.0f + 2.0f * p;
        case Shape::square:
            return p < 0.5f ? 1.0f : -1.0f;
        case Shape::sampleHold:
            return heldValue_;
        }
        return 0.0f;
    }

private:
    Rng rng_{0x51f0u};
    double sampleRate_ = 0.0;
    double phase_ = 0.0;
    float rateHz_ = 1.0f;
    float heldValue_ = 0.0f;
    Shape shape_ = Shape::sine;
};

} // namespace soundx::engine
