#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
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

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    soundx::engine::Wavetable wavetable_ = soundx::engine::Wavetable::makeSineSaw();
    juce::Synthesiser synth_;
    juce::AudioProcessorValueTreeState apvts_;

    std::atomic<float>* gain_ = nullptr;
    std::atomic<float>* attack_ = nullptr;
    std::atomic<float>* decay_ = nullptr;
    std::atomic<float>* sustain_ = nullptr;
    std::atomic<float>* release_ = nullptr;
    std::atomic<float>* position_ = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SoundXAudioProcessor)
};
