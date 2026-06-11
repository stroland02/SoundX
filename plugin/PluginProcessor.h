#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <array>
#include <memory>
#include "engine/Lfo.h"
#include "engine/SampleData.h"
#include "engine/effects/Chorus.h"
#include "engine/effects/Distortion.h"
#include "engine/effects/MultibandComp.h"
#include "engine/effects/Reverb.h"
#include "engine/effects/StereoDelay.h"
#include "engine/SpectralModel.h"
#include "engine/Wavetable.h"

class SoundXAudioProcessor : public juce::AudioProcessor {
public:
    static constexpr int kNumVoices = 16;

    SoundXAudioProcessor();

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "SoundX"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& apvts() { return apvts_; }

    static constexpr int kNumSlots = 2;
    static constexpr int kNumLfos = 3;
    static constexpr int kNumMacros = 4;

    // Swaps a slot's sample + derived wavetable bank + spectral model. Called
    // from the message thread (file import) or tests; suspends processing
    // during the swap.
    void applySample(int slot, std::shared_ptr<const soundx::engine::SampleData> sample,
                     const juce::String& name);

    // Kicks off async decode+import of an audio file into a slot (background thread).
    void loadSampleFile(int slot, const juce::File& file);

    juce::String currentSampleName(int slot) const { return slots_[size_t(slot)].name; }

    // Visualizer tap: UI thread pulls mono post-FX output samples. Lock-free.
    int popVisualizerSamples(float* dest, int maxSamples);
    float currentMorph() const { return morph_ != nullptr ? morph_->load() : 0.0f; }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    soundx::engine::Wavetable wavetable_ = soundx::engine::Wavetable::makeSineSaw();
    juce::Synthesiser synth_;
    juce::AudioProcessorValueTreeState apvts_;

    void rebindVoiceSources();

    struct SlotAssets {
        std::shared_ptr<const soundx::engine::SampleData> sample;      // grain source
        std::unique_ptr<soundx::engine::Wavetable> wavetable;          // from sample
        std::unique_ptr<soundx::engine::SpectralModel> model;          // from sample
        juce::String name;
    };
    struct SlotParams {
        std::atomic<float>* mode = nullptr;
        std::atomic<float>* position = nullptr;
        std::atomic<float>* grainsize = nullptr;
        std::atomic<float>* density = nullptr;
        std::atomic<float>* spray = nullptr;
        std::atomic<float>* stretch = nullptr;
    };

    std::array<SlotAssets, kNumSlots> slots_;
    std::array<SlotParams, kNumSlots> slotParams_;
    juce::ThreadPool importPool_{1};

    std::atomic<float>* gain_ = nullptr;
    std::atomic<float>* attack_ = nullptr;
    std::atomic<float>* decay_ = nullptr;
    std::atomic<float>* sustain_ = nullptr;
    std::atomic<float>* release_ = nullptr;
    std::atomic<float>* morph_ = nullptr;

    struct ModSourceParams {
        std::atomic<float>* dest = nullptr;
        std::atomic<float>* amount = nullptr;
        std::atomic<float>* rate = nullptr;  // LFOs only
        std::atomic<float>* shape = nullptr; // LFOs only
        std::atomic<float>* value = nullptr; // macros only
    };
    std::array<ModSourceParams, kNumLfos> lfoParams_;
    std::array<ModSourceParams, kNumMacros> macroParams_;
    std::array<soundx::engine::Lfo, kNumLfos> lfos_;

    soundx::engine::Distortion distortion_;
    soundx::engine::MultibandComp comp_;
    soundx::engine::Chorus chorus_;
    soundx::engine::StereoDelay delay_;
    soundx::engine::Reverb reverb_;
    struct FxParams {
        std::atomic<float>* on = nullptr;
        std::atomic<float>* p1 = nullptr;
        std::atomic<float>* p2 = nullptr;
        std::atomic<float>* p3 = nullptr;
    };
    FxParams distParams_, compParams_, chorusParams_, delayParams_, reverbParams_;

    juce::AbstractFifo vizFifo_{1 << 14};
    std::vector<float> vizBuffer_ = std::vector<float>(size_t(1 << 14), 0.0f);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SoundXAudioProcessor)
};
