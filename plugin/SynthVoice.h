#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <cmath>
#include <numbers>
#include <vector>
#include "engine/GranularVoice.h"
#include "engine/SpectralVoice.h"
#include "engine/WavetableVoice.h"

class SynthSound : public juce::SynthesiserSound {
public:
    bool appliesToNote(int) override { return true; }
    bool appliesToChannel(int) override { return true; }
};

// Bridges juce::Synthesiser to two engine slots (A and B) blended by morph.
// Both slots receive noteOn so morph automation mid-note always has material;
// a slot whose gain is ~0 is skipped (its envelope freezes and resumes
// gracefully if morph brings it back).
//
// Blend strategy: spectral+spectral morphs the partial data inside slot A's
// voice (frequencies glide); wavetable+wavetable crossfades linearly (phase-
// locked sources blend coherently); mixed modes use an equal-power crossfade.
class SynthVoice : public juce::SynthesiserVoice {
public:
    enum class Mode { wavetable = 0, granular = 1, spectral = 2 };
    static constexpr int kNumSlots = 2;

    explicit SynthVoice(const soundx::engine::Wavetable& factoryBank)
        : slots_{{Slot{factoryBank}, Slot{factoryBank}}} {}

    void prepare(double sampleRate, int maxBlockSize) {
        for (auto& s : slots_) {
            s.wt.setSampleRate(sampleRate);
            s.gran.setSampleRate(sampleRate);
            s.spec.setSampleRate(sampleRate);
        }
        scratch_.assign(size_t(maxBlockSize), 0.0f);
        mix_.assign(size_t(maxBlockSize), 0.0f);
    }

    void setMorph(float morph01) { morph_ = juce::jlimit(0.0f, 1.0f, morph01); }

    void setSharedParams(float a, float d, float s, float r) {
        for (auto& slot : slots_) {
            slot.wt.setParams(a, d, s, r, slot.position);
            slot.gran.setEnvParams(a, d, s, r);
            slot.spec.setEnvParams(a, d, s, r);
        }
    }

    void setSlotParams(int slot, Mode mode, float position, float grainSizeMs,
                       float densityHz, float spray, float stretch) {
        auto& s = slots_[size_t(slot)];
        s.mode = mode;
        s.position = position;
        s.gran.setGrainParams(grainSizeMs, densityHz, spray, position);
        s.spec.setSpectralParams(stretch);
    }

    // Safe only while the processor has rendering suspended.
    void setSlotSources(int slot, const soundx::engine::Wavetable* wavetable,
                        const soundx::engine::SampleData* sample,
                        const soundx::engine::SpectralModel* model) {
        auto& s = slots_[size_t(slot)];
        s.wt.setWavetables(wavetable, nullptr);
        s.gran.setSample(sample);
        s.spec.setModel(model);
        s.model = model;
        // keep slot A's morph target current
        slots_[0].spec.setModelB(slots_[1].model);
    }

    bool canPlaySound(juce::SynthesiserSound* sound) override {
        return dynamic_cast<SynthSound*>(sound) != nullptr;
    }

    void startNote(int midiNote, float velocity, juce::SynthesiserSound*, int) override {
        for (auto& s : slots_) {
            s.active = s.pick();
            s.active->noteOn(midiNote, velocity);
        }
    }

    void stopNote(float, bool allowTailOff) override {
        bool anyActive = false;
        for (auto& s : slots_) {
            if (s.active == nullptr)
                continue;
            if (allowTailOff) {
                s.active->noteOff();
                anyActive = anyActive || s.active->isActive();
            } else {
                s.active->kill();
            }
        }
        if (!anyActive)
            clearCurrentNote();
    }

    void pitchWheelMoved(int) override {}
    void controllerMoved(int, int) override {}

    void renderNextBlock(juce::AudioBuffer<float>& output, int startSample, int numSamples) override {
        if (slots_[0].active == nullptr && slots_[1].active == nullptr) {
            clearCurrentNote();
            return;
        }
        // Defensive: never trust the host to honor the prepared block size.
        numSamples = std::min(numSamples, int(mix_.size()));

        const bool bothSpectral = slots_[0].mode == Mode::spectral
                               && slots_[1].mode == Mode::spectral;
        const bool bothWavetable = slots_[0].mode == Mode::wavetable
                                && slots_[1].mode == Mode::wavetable;

        float gainA, gainB;
        if (bothSpectral) {
            // true morph happens inside slot A's spectral voice
            gainA = morph_ >= 0.9999f ? 0.0f : 1.0f;
            gainB = morph_ >= 0.9999f ? 1.0f : 0.0f;
        } else if (bothWavetable) {
            gainA = 1.0f - morph_;
            gainB = morph_;
        } else {
            constexpr float halfPi = float(std::numbers::pi) * 0.5f;
            gainA = std::cos(morph_ * halfPi);
            gainB = std::sin(morph_ * halfPi);
        }
        slots_[0].spec.setMorph(bothSpectral ? morph_ : 0.0f);

        std::fill_n(mix_.data(), size_t(numSamples), 0.0f);
        const float gains[kNumSlots] = {gainA, gainB};
        bool anyActive = false;
        for (int idx = 0; idx < kNumSlots; ++idx) {
            auto& s = slots_[size_t(idx)];
            if (s.active == nullptr)
                continue;
            anyActive = anyActive || s.active->isActive();
            if (gains[idx] < 1.0e-4f || !s.active->isActive())
                continue;
            std::fill_n(scratch_.data(), size_t(numSamples), 0.0f);
            s.active->render(scratch_.data(), numSamples);
            for (int i = 0; i < numSamples; ++i)
                mix_[size_t(i)] += scratch_[size_t(i)] * gains[idx];
        }

        for (int ch = 0; ch < output.getNumChannels(); ++ch)
            output.addFrom(ch, startSample, mix_.data(), numSamples);

        if (!anyActive)
            clearCurrentNote();
    }

private:
    struct Slot {
        explicit Slot(const soundx::engine::Wavetable& bank) : wt(bank) {}
        soundx::engine::WavetableVoice wt;
        soundx::engine::GranularVoice gran;
        soundx::engine::SpectralVoice spec;
        soundx::engine::SoundSource* active = nullptr;
        const soundx::engine::SpectralModel* model = nullptr;
        Mode mode = Mode::wavetable;
        float position = 0.0f;

        soundx::engine::SoundSource* pick() {
            switch (mode) {
            case Mode::granular: return &gran;
            case Mode::spectral: return &spec;
            case Mode::wavetable: break;
            }
            return &wt;
        }
    };

    std::array<Slot, kNumSlots> slots_;
    float morph_ = 0.0f;
    std::vector<float> scratch_, mix_;
};
