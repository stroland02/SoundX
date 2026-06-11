#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <memory>
#include "engine/SampleData.h"
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

    // Swaps in a new sample + derived wavetable bank. Called from the message
    // thread (file import) or tests; suspends processing during the swap.
    void applySample(std::shared_ptr<const soundx::engine::SampleData> sample,
                     const juce::String& name);

    // Kicks off async decode+import of an audio file (background thread).
    void loadSampleFile(const juce::File& file);

    juce::String currentSampleName() const { return sampleName_; }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    soundx::engine::Wavetable wavetable_ = soundx::engine::Wavetable::makeSineSaw();
    juce::Synthesiser synth_;
    juce::AudioProcessorValueTreeState apvts_;

    void rebindVoiceSources();

    std::shared_ptr<const soundx::engine::SampleData> sample_;       // grain source
    std::unique_ptr<soundx::engine::Wavetable> importedWavetable_;   // from sample
    juce::String sampleName_;
    juce::ThreadPool importPool_{1};

    std::atomic<float>* gain_ = nullptr;
    std::atomic<float>* attack_ = nullptr;
    std::atomic<float>* decay_ = nullptr;
    std::atomic<float>* sustain_ = nullptr;
    std::atomic<float>* release_ = nullptr;
    std::atomic<float>* position_ = nullptr;
    std::atomic<float>* mode_ = nullptr;
    std::atomic<float>* grainsize_ = nullptr;
    std::atomic<float>* density_ = nullptr;
    std::atomic<float>* spray_ = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SoundXAudioProcessor)
};
