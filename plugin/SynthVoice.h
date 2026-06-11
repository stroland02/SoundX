#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <vector>
#include "engine/GranularVoice.h"
#include "engine/WavetableVoice.h"

class SynthSound : public juce::SynthesiserSound {
public:
    bool appliesToNote(int) override { return true; }
    bool appliesToChannel(int) override { return true; }
};

// Bridges juce::Synthesiser to the engine voices. Owns one voice per engine
// mode; `mode` chooses which one a new note plays on. A note that started in
// one mode finishes its tail on that engine even after a mode switch.
class SynthVoice : public juce::SynthesiserVoice {
public:
    enum class Mode { wavetable = 0, granular = 1 };

    explicit SynthVoice(const soundx::engine::Wavetable& wavetable) : wtVoice_(wavetable) {}

    // Pre-allocates the scratch buffer. Call from prepareToPlay, never from audio thread.
    void prepare(double sampleRate, int maxBlockSize) {
        wtVoice_.setSampleRate(sampleRate);
        granVoice_.setSampleRate(sampleRate);
        scratch_.assign(size_t(maxBlockSize), 0.0f);
    }

    void setMode(Mode m) { mode_ = m; }

    void setParams(float a, float d, float s, float r, float position,
                   float grainSizeMs, float densityHz, float spray) {
        wtVoice_.setParams(a, d, s, r, position);
        granVoice_.setEnvParams(a, d, s, r);
        granVoice_.setGrainParams(grainSizeMs, densityHz, spray, position);
    }

    // Safe only while the processor has rendering suspended.
    void setSources(const soundx::engine::Wavetable* wavetable,
                    const soundx::engine::SampleData* sample) {
        wtVoice_.setWavetable(wavetable);
        granVoice_.setSample(sample);
    }

    bool canPlaySound(juce::SynthesiserSound* sound) override {
        return dynamic_cast<SynthSound*>(sound) != nullptr;
    }

    void startNote(int midiNote, float velocity, juce::SynthesiserSound*, int) override {
        activeSource_ = (mode_ == Mode::granular)
            ? static_cast<soundx::engine::SoundSource*>(&granVoice_)
            : static_cast<soundx::engine::SoundSource*>(&wtVoice_);
        activeSource_->noteOn(midiNote, velocity);
    }

    void stopNote(float, bool allowTailOff) override {
        if (activeSource_ == nullptr) {
            clearCurrentNote();
            return;
        }
        if (allowTailOff) {
            activeSource_->noteOff();
        } else {
            activeSource_->kill();
            clearCurrentNote();
        }
    }

    void pitchWheelMoved(int) override {}
    void controllerMoved(int, int) override {}

    void renderNextBlock(juce::AudioBuffer<float>& output, int startSample, int numSamples) override {
        if (activeSource_ == nullptr || !activeSource_->isActive()) {
            clearCurrentNote();
            return;
        }
        // Defensive: never trust the host to honor the prepared block size.
        numSamples = std::min(numSamples, int(scratch_.size()));
        std::fill_n(scratch_.data(), size_t(numSamples), 0.0f);
        activeSource_->render(scratch_.data(), numSamples);
        for (int ch = 0; ch < output.getNumChannels(); ++ch)
            output.addFrom(ch, startSample, scratch_.data(), numSamples);
        if (!activeSource_->isActive())
            clearCurrentNote();
    }

private:
    soundx::engine::WavetableVoice wtVoice_;
    soundx::engine::GranularVoice granVoice_;
    soundx::engine::SoundSource* activeSource_ = nullptr;
    Mode mode_ = Mode::wavetable;
    std::vector<float> scratch_;
};
