#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <vector>
#include "engine/WavetableVoice.h"

class SynthSound : public juce::SynthesiserSound {
public:
    bool appliesToNote(int) override { return true; }
    bool appliesToChannel(int) override { return true; }
};

class SynthVoice : public juce::SynthesiserVoice {
public:
    explicit SynthVoice(const soundx::engine::Wavetable& wavetable) : voice_(wavetable) {}

    // Pre-allocates the scratch buffer. Call from prepareToPlay, never from audio thread.
    void prepare(double sampleRate, int maxBlockSize) {
        voice_.setSampleRate(sampleRate);
        scratch_.assign(size_t(maxBlockSize), 0.0f);
    }

    void setParams(float a, float d, float s, float r, float position) {
        voice_.setParams(a, d, s, r, position);
    }

    bool canPlaySound(juce::SynthesiserSound* sound) override {
        return dynamic_cast<SynthSound*>(sound) != nullptr;
    }

    void startNote(int midiNote, float velocity, juce::SynthesiserSound*, int) override {
        voice_.noteOn(midiNote, velocity);
    }

    void stopNote(float, bool allowTailOff) override {
        if (allowTailOff) {
            voice_.noteOff();
        } else {
            voice_.kill();
            clearCurrentNote();
        }
    }

    void pitchWheelMoved(int) override {}
    void controllerMoved(int, int) override {}

    void renderNextBlock(juce::AudioBuffer<float>& output, int startSample, int numSamples) override {
        if (!voice_.isActive()) {
            clearCurrentNote();
            return;
        }
        std::fill_n(scratch_.data(), size_t(numSamples), 0.0f);
        voice_.render(scratch_.data(), numSamples);
        for (int ch = 0; ch < output.getNumChannels(); ++ch)
            output.addFrom(ch, startSample, scratch_.data(), numSamples);
        if (!voice_.isActive())
            clearCurrentNote();
    }

private:
    soundx::engine::WavetableVoice voice_;
    std::vector<float> scratch_;
};
