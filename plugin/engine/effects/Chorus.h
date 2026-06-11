#pragma once
// JUCE-free stereo chorus: one modulated delay line per channel, right channel
// LFO offset 90 degrees for width. process() is RT-safe; prepare() allocates.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <vector>

namespace soundx::engine {

class Chorus {
public:
    void prepare(double sampleRate, int) {
        sampleRate_ = sampleRate;
        const auto size = std::size_t(sampleRate * 0.06) + 4; // 60 ms headroom
        for (auto& line : lines_) {
            line.buf.assign(size, 0.0f);
            line.write = 0;
        }
        phase_ = 0.0;
    }

    void setParams(float rateHz, float depthMs, float mix01) {
        rateHz_ = std::clamp(rateHz, 0.1f, 5.0f);
        depthMs_ = std::clamp(depthMs, 1.0f, 15.0f);
        mix_ = std::clamp(mix01, 0.0f, 1.0f);
    }

    void process(float* l, float* r, int n) noexcept {
        if (mix_ <= 0.0f || lines_[0].buf.empty())
            return;
        constexpr float kBaseMs = 20.0f;
        const double phaseInc = double(rateHz_) / sampleRate_;
        for (int i = 0; i < n; ++i) {
            const float lfoL = float(std::sin(2.0 * std::numbers::pi * phase_));
            const float lfoR = float(std::sin(2.0 * std::numbers::pi * (phase_ + 0.25)));
            phase_ += phaseInc;
            phase_ -= std::floor(phase_);

            l[i] = tap(lines_[0], l[i], kBaseMs + depthMs_ * lfoL);
            r[i] = tap(lines_[1], r[i], kBaseMs + depthMs_ * lfoR);
        }
    }

private:
    struct Line {
        std::vector<float> buf;
        std::size_t write = 0;
    };

    float tap(Line& line, float x, float delayMs) noexcept {
        line.buf[line.write] = x;
        const float delaySamples = delayMs * float(sampleRate_) * 0.001f;
        const float readPos = float(line.write) - delaySamples;
        const auto size = float(line.buf.size());
        const float wrapped = readPos < 0.0f ? readPos + size : readPos;
        const auto i0 = std::size_t(wrapped) % line.buf.size();
        const auto i1 = (i0 + 1) % line.buf.size();
        const float frac = wrapped - std::floor(wrapped);
        const float wet = line.buf[i0] * (1.0f - frac) + line.buf[i1] * frac;
        line.write = (line.write + 1) % line.buf.size();
        return x + (wet - x) * mix_ * 0.5f; // 50/50 at full mix keeps level sane
    }

    std::array<Line, 2> lines_;
    double sampleRate_ = 44100.0;
    double phase_ = 0.0;
    float rateHz_ = 1.0f, depthMs_ = 8.0f, mix_ = 0.5f;
};

} // namespace soundx::engine
