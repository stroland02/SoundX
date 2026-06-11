#pragma once
// JUCE-free granular engine voice. render() is RT-safe: fixed grain pool,
// no allocation, no locks. setSample() must only be called while the voice
// is not rendering (the plugin swaps samples under suspendProcessing).
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <numbers>
#include "Adsr.h"
#include "Rng.h"
#include "SampleData.h"
#include "SoundSource.h"

namespace soundx::engine {

class GranularVoice : public SoundSource {
public:
    static constexpr std::size_t kMaxGrains = 32;
    static constexpr float kMinGrainMs = 5.0f, kMaxGrainMs = 500.0f;
    static constexpr float kMinDensityHz = 0.5f, kMaxDensityHz = 100.0f;

    void setSampleRate(double sampleRate) override {
        sampleRate_ = sampleRate;
        env_.setSampleRate(sampleRate);
    }

    void setSample(const SampleData* sample) { sample_ = sample; }

    void setEnvParams(float a, float d, float s, float r) { env_.setParams(a, d, s, r); }

    void setGrainParams(float grainSizeMs, float densityHz, float spray01, float position01) {
        grainSizeMs_ = std::clamp(grainSizeMs, kMinGrainMs, kMaxGrainMs);
        densityHz_ = std::clamp(densityHz, kMinDensityHz, kMaxDensityHz);
        spray_ = std::clamp(spray01, 0.0f, 1.0f);
        position_ = std::clamp(position01, 0.0f, 1.0f);
    }

    void noteOn(int midiNote, float velocity01) override {
        velocity_ = velocity01;
        // note 60 plays the sample at its recorded pitch
        rate_ = std::pow(2.0, double(midiNote - 60) / 12.0);
        samplesToNextGrain_ = 0; // spawn immediately
        env_.noteOn();
    }

    void noteOff() override { env_.noteOff(); }

    void kill() override {
        env_.reset();
        for (auto& g : grains_)
            g.active = false;
    }

    bool isActive() const override { return env_.isActive(); }

    void render(float* dest, int numSamples) override {
        if (!isActive() || sample_ == nullptr || sample_->samples.size() < 2 || sampleRate_ <= 0.0) {
            // keep envelope time flowing so a sample-less noteOn still expires
            for (int i = 0; i < numSamples; ++i)
                env_.nextLevel();
            return;
        }
        const auto& src = sample_->samples;
        const double srcLen = double(src.size());
        const double interOnset = sampleRate_ / double(densityHz_);

        for (int i = 0; i < numSamples; ++i) {
            const float envLevel = env_.nextLevel();
            if (envLevel <= 0.0f && !env_.isActive())
                break;

            if (--samplesToNextGrain_ <= 0) {
                spawnGrain(srcLen);
                samplesToNextGrain_ = int(interOnset);
            }

            float mix = 0.0f;
            for (auto& g : grains_) {
                if (!g.active)
                    continue;
                // Hann window over grain age
                const float w = 0.5f * (1.0f - std::cos(2.0f * float(std::numbers::pi)
                                * float(g.age) / float(g.length)));
                const auto i0 = std::size_t(g.srcPos);
                const auto i1 = std::min(i0 + 1, src.size() - 1);
                const float frac = float(g.srcPos - double(i0));
                mix += (src[i0] * (1.0f - frac) + src[i1] * frac) * w;

                g.srcPos += g.rate;
                if (++g.age >= g.length || g.srcPos >= srcLen - 1.0)
                    g.active = false;
            }
            // 1/sqrt(maxOverlap) keeps dense clouds from clipping
            const float overlap = std::max(1.0f, densityHz_ * grainSizeMs_ * 0.001f);
            dest[i] += mix * envLevel * velocity_ / std::sqrt(overlap);
        }
    }

private:
    struct Grain {
        double srcPos = 0.0;
        double rate = 1.0;
        std::size_t age = 0, length = 0;
        bool active = false;
    };

    void spawnGrain(double srcLen) {
        for (auto& g : grains_) {
            if (g.active)
                continue;
            const double lengthSamples = double(grainSizeMs_) * 0.001 * sampleRate_;
            const double sprayOffset = double(spray_) * (rng_.next01() * 2.0f - 1.0f) * srcLen * 0.5;
            double start = double(position_) * (srcLen - lengthSamples - 1.0) + sprayOffset;
            start = std::clamp(start, 0.0, std::max(0.0, srcLen - 2.0));
            g.srcPos = start;
            g.rate = rate_;
            g.age = 0;
            g.length = std::max<std::size_t>(8, std::size_t(lengthSamples));
            g.active = true;
            return;
        }
        // pool exhausted: skip this onset (graceful degradation)
    }

    std::array<Grain, kMaxGrains> grains_{};
    Adsr env_;
    Rng rng_;
    const SampleData* sample_ = nullptr;
    double sampleRate_ = 0.0;
    double rate_ = 1.0;
    float velocity_ = 0.0f;
    float grainSizeMs_ = 100.0f, densityHz_ = 30.0f, spray_ = 0.2f, position_ = 0.0f;
    int samplesToNextGrain_ = 0;
};

} // namespace soundx::engine
