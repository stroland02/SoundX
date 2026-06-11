#pragma once
// JUCE-free. render() is RT-safe: writes additively into caller's buffer.
#include <cmath>
#include "Adsr.h"
#include "SoundSource.h"
#include "Wavetable.h"
#include "WavetableOscillator.h"

namespace soundx::engine {

class WavetableVoice : public SoundSource {
public:
    explicit WavetableVoice(const Wavetable& wavetable) : osc_(wavetable) {}

    void setSampleRate(double sampleRate) override {
        osc_.setSampleRate(sampleRate);
        env_.setSampleRate(sampleRate);
    }

    void setParams(float attackS, float decayS, float sustain01, float releaseS, float position01) {
        env_.setParams(attackS, decayS, sustain01, releaseS);
        osc_.setPosition(position01);
    }

    // Retarget to a different (already-built) bank. Safe between blocks only.
    void setWavetable(const Wavetable* wavetable) { osc_.setTable(wavetable); }

    void noteOn(int midiNote, float velocity01) override {
        velocity_ = velocity01;
        osc_.setFrequency(midiNoteToHz(midiNote));
        osc_.reset();
        env_.noteOn();
    }

    void noteOff() override { env_.noteOff(); }
    void kill() override { env_.reset(); }
    bool isActive() const override { return env_.isActive(); }

    void render(float* dest, int numSamples) noexcept override {
        if (!isActive())
            return;
        for (int i = 0; i < numSamples; ++i)
            dest[i] += osc_.nextSample() * env_.nextLevel() * velocity_;
    }

    static float midiNoteToHz(int midiNote) {
        return 440.0f * std::pow(2.0f, float(midiNote - 69) / 12.0f);
    }

private:
    WavetableOscillator osc_;
    Adsr env_;
    float velocity_ = 0.0f;
};

} // namespace soundx::engine
