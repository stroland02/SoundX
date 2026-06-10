#include "PluginProcessor.h"
#include "PluginEditor.h"

SoundXAudioProcessor::SoundXAudioProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)) {}

void SoundXAudioProcessor::prepareToPlay(double, int) {}

void SoundXAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
}

bool SoundXAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        || layouts.getMainOutputChannelSet() == juce::AudioChannelSet::mono();
}

juce::AudioProcessorEditor* SoundXAudioProcessor::createEditor() {
    return new SoundXAudioProcessorEditor(*this);
}

void SoundXAudioProcessor::getStateInformation(juce::MemoryBlock&) {}
void SoundXAudioProcessor::setStateInformation(const void*, int) {}

// JUCE plugin entry point
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new SoundXAudioProcessor();
}
