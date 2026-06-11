#pragma once
// JUCE-free OTT-style 3-band up/down compressor. process() is RT-safe.
// Band split uses one-pole crossovers arranged so the bands sum exactly to
// the input (low + mid + high == x by construction), making depth 0 a true
// passthrough.
#include <algorithm>
#include <cmath>

namespace soundx::engine {

class MultibandComp {
public:
    void prepare(double sampleRate, int) {
        sampleRate_ = sampleRate;
        const auto coeff = [this](double hz) {
            return float(1.0 - std::exp(-2.0 * 3.14159265358979 * hz / sampleRate_));
        };
        kLow_ = coeff(200.0);
        kHigh_ = coeff(2000.0);
        attack_ = coeff(1000.0 / 5.0);   // ~5 ms
        release_ = coeff(1000.0 / 100.0); // ~100 ms
        for (auto& s : state_)
            s = {};
    }

    void setParams(float depth01) { depth_ = std::clamp(depth01, 0.0f, 1.0f); }

    void process(float* l, float* r, int n) noexcept {
        if (depth_ <= 0.0f)
            return;
        processChannel(l, n, state_[0]);
        processChannel(r, n, state_[1]);
    }

private:
    struct Channel {
        float lp200 = 0.0f, lp2k = 0.0f;
        float env[3] = {0.0f, 0.0f, 0.0f};
    };

    void processChannel(float* x, int n, Channel& ch) noexcept {
        constexpr float kRef = 0.25f;     // target band level
        constexpr float kFloor = 1e-4f;   // below this, stop applying upward gain
        const float exponent = 0.6f * depth_;
        for (int i = 0; i < n; ++i) {
            ch.lp200 += (x[i] - ch.lp200) * kLow_;
            ch.lp2k += (x[i] - ch.lp2k) * kHigh_;
            float bands[3];
            bands[0] = ch.lp200;
            bands[1] = ch.lp2k - ch.lp200;
            bands[2] = x[i] - ch.lp2k;

            float out = 0.0f;
            for (int b = 0; b < 3; ++b) {
                const float mag = std::abs(bands[b]);
                float& env = ch.env[b];
                env += (mag - env) * (mag > env ? attack_ : release_);
                float gain = 1.0f;
                if (env > kFloor)
                    gain = std::clamp(std::pow(kRef / env, exponent), 0.25f, 4.0f);
                out += bands[b] * gain;
            }
            x[i] = out;
        }
    }

    double sampleRate_ = 44100.0;
    float kLow_ = 0.0f, kHigh_ = 0.0f, attack_ = 0.0f, release_ = 0.0f;
    float depth_ = 0.5f;
    Channel state_[2];
};

} // namespace soundx::engine
