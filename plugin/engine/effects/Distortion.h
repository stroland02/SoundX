#pragma once
// JUCE-free. Normalized tanh waveshaper. process() is RT-safe.
#include <algorithm>
#include <cmath>

namespace soundx::engine {

class Distortion {
public:
    void prepare(double, int) {}

    void setParams(float drive, float mix01) {
        drive_ = std::clamp(drive, 1.0f, 20.0f);
        mix_ = std::clamp(mix01, 0.0f, 1.0f);
        norm_ = 1.0f / std::tanh(drive_);
    }

    void process(float* l, float* r, int n) noexcept {
        if (mix_ <= 0.0f)
            return;
        for (int i = 0; i < n; ++i) {
            l[i] += (std::tanh(l[i] * drive_) * norm_ - l[i]) * mix_;
            r[i] += (std::tanh(r[i] * drive_) * norm_ - r[i]) * mix_;
        }
    }

private:
    float drive_ = 4.0f, mix_ = 1.0f, norm_ = 1.0f;
};

} // namespace soundx::engine
