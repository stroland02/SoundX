#pragma once
// JUCE-free stereo delay with feedback. process() is RT-safe; prepare() allocates.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

namespace soundx::engine {

class StereoDelay {
public:
    void prepare(double sampleRate, int) {
        sampleRate_ = sampleRate;
        const auto size = std::size_t(sampleRate * 2.2) + 4;
        for (auto& line : lines_) {
            line.buf.assign(size, 0.0f);
            line.write = 0;
        }
    }

    void setParams(float timeMs, float feedback01, float mix01) {
        timeMs_ = std::clamp(timeMs, 10.0f, 2000.0f);
        feedback_ = std::clamp(feedback01, 0.0f, 0.95f);
        mix_ = std::clamp(mix01, 0.0f, 1.0f);
    }

    void process(float* l, float* r, int n) noexcept {
        if (mix_ <= 0.0f || lines_[0].buf.empty())
            return;
        const float delaySamples = timeMs_ * float(sampleRate_) * 0.001f;
        for (int i = 0; i < n; ++i) {
            l[i] = tap(lines_[0], l[i], delaySamples);
            r[i] = tap(lines_[1], r[i], delaySamples);
        }
    }

private:
    struct Line {
        std::vector<float> buf;
        std::size_t write = 0;
    };

    float tap(Line& line, float x, float delaySamples) noexcept {
        const float readPos = float(line.write) - delaySamples;
        const auto size = float(line.buf.size());
        const float wrapped = readPos < 0.0f ? readPos + size : readPos;
        const auto i0 = std::size_t(wrapped) % line.buf.size();
        const auto i1 = (i0 + 1) % line.buf.size();
        const float frac = wrapped - std::floor(wrapped);
        const float wet = line.buf[i0] * (1.0f - frac) + line.buf[i1] * frac;
        line.buf[line.write] = x + wet * feedback_;
        line.write = (line.write + 1) % line.buf.size();
        return x + (wet - x) * mix_;
    }

    std::array<Line, 2> lines_;
    double sampleRate_ = 44100.0;
    float timeMs_ = 350.0f, feedback_ = 0.35f, mix_ = 0.3f;
};

} // namespace soundx::engine
