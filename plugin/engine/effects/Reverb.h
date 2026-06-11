#pragma once
// JUCE-free Freeverb-topology reverb: 8 parallel feedback combs with damping
// into 4 serial allpasses per channel, right channel offset for stereo width.
// process() is RT-safe; prepare() allocates.
#include <algorithm>
#include <array>
#include <cstddef>
#include <vector>

namespace soundx::engine {

class Reverb {
public:
    void prepare(double sampleRate, int) {
        const double scale = sampleRate / 44100.0;
        constexpr std::array<int, 8> combTunings = {1116, 1188, 1277, 1356,
                                                    1422, 1491, 1557, 1617};
        constexpr std::array<int, 4> allpassTunings = {556, 441, 341, 225};
        constexpr int kStereoSpread = 23;

        for (int ch = 0; ch < 2; ++ch) {
            auto& c = channels_[size_t(ch)];
            const int spread = ch == 0 ? 0 : kStereoSpread;
            for (std::size_t i = 0; i < c.combs.size(); ++i) {
                c.combs[i].buf.assign(std::size_t(double(combTunings[i] + spread) * scale), 0.0f);
                c.combs[i].pos = 0;
                c.combs[i].filterState = 0.0f;
            }
            for (std::size_t i = 0; i < c.allpasses.size(); ++i) {
                c.allpasses[i].buf.assign(std::size_t(double(allpassTunings[i] + spread) * scale), 0.0f);
                c.allpasses[i].pos = 0;
            }
        }
    }

    void setParams(float size01, float damp01, float mix01) {
        feedback_ = 0.7f + 0.28f * std::clamp(size01, 0.0f, 1.0f);
        damp_ = 0.4f * std::clamp(damp01, 0.0f, 1.0f);
        mix_ = std::clamp(mix01, 0.0f, 1.0f);
    }

    void process(float* l, float* r, int n) noexcept {
        if (mix_ <= 0.0f || channels_[0].combs[0].buf.empty())
            return;
        for (int i = 0; i < n; ++i) {
            l[i] = processSample(channels_[0], l[i]);
            r[i] = processSample(channels_[1], r[i]);
        }
    }

private:
    struct Comb {
        std::vector<float> buf;
        std::size_t pos = 0;
        float filterState = 0.0f;
    };
    struct Allpass {
        std::vector<float> buf;
        std::size_t pos = 0;
    };
    struct Channel {
        std::array<Comb, 8> combs;
        std::array<Allpass, 4> allpasses;
    };

    float processSample(Channel& ch, float x) noexcept {
        const float input = x * 0.015f; // Freeverb input gain
        float wet = 0.0f;
        for (auto& comb : ch.combs) {
            const float out = comb.buf[comb.pos];
            comb.filterState += (out - comb.filterState) * (1.0f - damp_);
            comb.buf[comb.pos] = input + comb.filterState * feedback_;
            comb.pos = (comb.pos + 1) % comb.buf.size();
            wet += out;
        }
        for (auto& ap : ch.allpasses) {
            const float buffered = ap.buf[ap.pos];
            ap.buf[ap.pos] = wet + buffered * 0.5f;
            wet = buffered - wet * 0.5f;
            ap.pos = (ap.pos + 1) % ap.buf.size();
        }
        return x + (wet - x) * mix_;
    }

    std::array<Channel, 2> channels_;
    float feedback_ = 0.84f, damp_ = 0.2f, mix_ = 0.3f;
};

} // namespace soundx::engine
