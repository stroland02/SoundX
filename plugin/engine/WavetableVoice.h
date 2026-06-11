#pragma once
// JUCE-free. render() is RT-safe: writes additively into caller's buffer.
#include <cmath>
#include "Adsr.h"
#include "Wavetable.h"
#include "WavetableOscillator.h"

namespace soundx::engine {

class WavetableVoice {
public:
    explicit WavetableVoice(const Wavetable& wavetable) : osc_(wavetable) {}

    void setSampleRate(double sampleRate) {
        osc_.setSampleRate(sampleRate);
        env_.setSampleRate(sampleRate);
    }

    void setParams(float attackS, float decayS, float sustain01, float releaseS, float position01) {
        env_.setParams(attackS, decayS, sustain01, releaseS);
        osc_.setPosition(position01);
    }

    void noteOn(int midiNote, float velocity01) {
        velocity_ = velocity01;
        osc_.setFrequency(midiNoteToHz(midiNote));
        osc_.reset();
        env_.noteOn();
    }

    void noteOff() { env_.noteOff(); }
    void kill() { env_.reset(); }
    bool isActive() const { return env_.isActive(); }

    void render(float* dest, int numSamples) noexcept {
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
