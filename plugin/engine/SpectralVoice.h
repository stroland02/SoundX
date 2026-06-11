#pragma once
// JUCE-free additive resynthesis voice. render() is RT-safe: fixed oscillator
// bank, no allocation, no locks. setModel() must only be called while the
// voice is not rendering (the plugin swaps models under suspendProcessing).
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <numbers>
#include "Adsr.h"
#include "SoundSource.h"
#include "SpectralModel.h"

namespace soundx::engine {

class SpectralVoice : public SoundSource {
public:
    static constexpr float kMaxStretch = 4.0f;

    void setSampleRate(double sampleRate) override {
        sampleRate_ = sampleRate;
        env_.setSampleRate(sampleRate);
    }

    void setModel(const SpectralModel* model) { model_ = model; }

    void setEnvParams(float a, float d, float s, float r) { env_.setParams(a, d, s, r); }

    // stretch: playback speed multiplier for the frame timeline; 0 freezes.
    void setSpectralParams(float stretch) {
        stretch_ = std::clamp(stretch, 0.0f, kMaxStretch);
    }

    void noteOn(int midiNote, float velocity01) override {
        velocity_ = velocity01;
        // note 60 plays the analyzed material at its original pitch
        pitchRatio_ = std::pow(2.0, double(midiNote - 60) / 12.0);
        framePos_ = 0.0;
        phases_.fill(0.0f);
        env_.noteOn();
    }

    void noteOff() override { env_.noteOff(); }
    void kill() override { env_.reset(); }
    bool isActive() const override { return env_.isActive(); }

    void render(float* dest, int numSamples) override {
        if (!isActive() || model_ == nullptr || model_->frames.empty() || sampleRate_ <= 0.0) {
            for (int i = 0; i < numSamples; ++i)
                env_.nextLevel(); // keep envelope time flowing
            return;
        }
        const auto& frames = model_->frames;
        const double frameAdvance = model_->framesPerSecond * double(stretch_) / sampleRate_;
        const double lastFrame = double(frames.size() - 1);
        constexpr float twoPi = 2.0f * float(std::numbers::pi);
        const float nyquist = float(sampleRate_) * 0.5f;

        for (int i = 0; i < numSamples; ++i) {
            const float envLevel = env_.nextLevel();
            if (envLevel <= 0.0f && !env_.isActive())
                break;

            const auto f0 = std::size_t(framePos_);
            const auto f1 = std::min(f0 + 1, frames.size() - 1);
            const float frac = float(framePos_ - double(f0));
            const auto& a = frames[f0].partials;
            const auto& b = frames[f1].partials;

            float mix = 0.0f;
            for (std::size_t p = 0; p < SpectralFrame::kMaxPartials; ++p) {
                const float amp = a[p].amp * (1.0f - frac) + b[p].amp * frac;
                if (amp < 1e-6f)
                    continue;
                const float freq = (a[p].freqHz * (1.0f - frac) + b[p].freqHz * frac)
                                 * float(pitchRatio_);
                if (freq <= 0.0f || freq >= nyquist)
                    continue;
                float& phase = phases_[p];
                mix += amp * std::sin(twoPi * phase);
                phase += freq / float(sampleRate_);
                phase -= std::floor(phase);
            }
            dest[i] += mix * envLevel * velocity_;

            framePos_ = std::min(framePos_ + frameAdvance, lastFrame);
        }
    }

private:
    std::array<float, SpectralFrame::kMaxPartials> phases_{};
    Adsr env_;
    const SpectralModel* model_ = nullptr;
    double sampleRate_ = 0.0;
    double pitchRatio_ = 1.0;
    double framePos_ = 0.0;
    float velocity_ = 0.0f;
    float stretch_ = 1.0f;
};

} // namespace soundx::engine
