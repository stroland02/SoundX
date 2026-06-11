#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "SynthVoice.h"

namespace {
juce::NormalisableRange<float> secondsRange() {
    juce::NormalisableRange<float> r(0.001f, 5.0f);
    r.setSkewForCentre(0.3f);
    return r;
}
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout SoundXAudioProcessor::createParameterLayout() {
    using P = juce::AudioParameterFloat;
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add(std::make_unique<P>(juce::ParameterID{"gain", 1}, "Gain",
                                   juce::NormalisableRange<float>(0.0f, 1.0f), 0.8f));
    layout.add(std::make_unique<P>(juce::ParameterID{"attack", 1}, "Attack", secondsRange(), 0.01f));
    layout.add(std::make_unique<P>(juce::ParameterID{"decay", 1}, "Decay", secondsRange(), 0.1f));
    layout.add(std::make_unique<P>(juce::ParameterID{"sustain", 1}, "Sustain",
                                   juce::NormalisableRange<float>(0.0f, 1.0f), 0.8f));
    layout.add(std::make_unique<P>(juce::ParameterID{"release", 1}, "Release", secondsRange(), 0.2f));
    layout.add(std::make_unique<P>(juce::ParameterID{"position", 1}, "Position",
                                   juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));
    return layout;
}

SoundXAudioProcessor::SoundXAudioProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts_(*this, nullptr, "PARAMS", createParameterLayout()) {
    gain_ = apvts_.getRawParameterValue("gain");
    attack_ = apvts_.getRawParameterValue("attack");
    decay_ = apvts_.getRawParameterValue("decay");
    sustain_ = apvts_.getRawParameterValue("sustain");
    release_ = apvts_.getRawParameterValue("release");
    position_ = apvts_.getRawParameterValue("position");

    synth_.addSound(new SynthSound());
    for (int i = 0; i < kNumVoices; ++i)
        synth_.addVoice(new SynthVoice(wavetable_));
}

void SoundXAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    synth_.setCurrentPlaybackSampleRate(sampleRate);
    for (int i = 0; i < synth_.getNumVoices(); ++i)
        if (auto* v = dynamic_cast<SynthVoice*>(synth_.getVoice(i)))
            v->prepare(sampleRate, samplesPerBlock);
}

void SoundXAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    const float a = attack_->load(), d = decay_->load(), s = sustain_->load(),
                r = release_->load(), pos = position_->load();
    for (int i = 0; i < synth_.getNumVoices(); ++i)
        if (auto* v = dynamic_cast<SynthVoice*>(synth_.getVoice(i)))
            v->setParams(a, d, s, r, pos);

    synth_.renderNextBlock(buffer, midi, 0, buffer.getNumSamples());
    buffer.applyGain(gain_->load());
}

bool SoundXAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        || layouts.getMainOutputChannelSet() == juce::AudioChannelSet::mono();
}

juce::AudioProcessorEditor* SoundXAudioProcessor::createEditor() {
    return new SoundXAudioProcessorEditor(*this);
}

void SoundXAudioProcessor::getStateInformation(juce::MemoryBlock& destData) {
    if (auto xml = apvts_.copyState().createXml())
        copyXmlToBinary(*xml, destData);
}

void SoundXAudioProcessor::setStateInformation(const void* data, int sizeInBytes) {
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        if (xml->hasTagName(apvts_.state.getType()))
            apvts_.replaceState(juce::ValueTree::fromXml(*xml));
}

// JUCE plugin entry point
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new SoundXAudioProcessor();
}
