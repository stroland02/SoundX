#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "SynthVoice.h"
#include "engine/SampleImporter.h"

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
    layout.add(std::make_unique<P>(juce::ParameterID{"morph", 1}, "Morph",
                                   juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));

    for (const auto* prefix : {"a", "b"}) {
        const juce::String p(prefix);
        const juce::String label = p.toUpperCase() + " ";
        layout.add(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID{p + "_mode", 1}, label + "Engine Mode",
            juce::StringArray{"Wavetable", "Granular", "Spectral"}, 0));
        layout.add(std::make_unique<P>(juce::ParameterID{p + "_position", 1}, label + "Position",
                                       juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));
        layout.add(std::make_unique<P>(juce::ParameterID{p + "_grainsize", 1}, label + "Grain Size",
                                       juce::NormalisableRange<float>(5.0f, 500.0f, 0.0f, 0.4f), 100.0f));
        layout.add(std::make_unique<P>(juce::ParameterID{p + "_density", 1}, label + "Density",
                                       juce::NormalisableRange<float>(0.5f, 100.0f, 0.0f, 0.4f), 30.0f));
        layout.add(std::make_unique<P>(juce::ParameterID{p + "_spray", 1}, label + "Spray",
                                       juce::NormalisableRange<float>(0.0f, 1.0f), 0.2f));
        juce::NormalisableRange<float> stretchRange(0.0f, 4.0f);
        stretchRange.setSkewForCentre(1.0f);
        layout.add(std::make_unique<P>(juce::ParameterID{p + "_stretch", 1}, label + "Stretch",
                                       stretchRange, 1.0f));
    }
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
    morph_ = apvts_.getRawParameterValue("morph");
    for (int slot = 0; slot < kNumSlots; ++slot) {
        const juce::String p = slot == 0 ? "a" : "b";
        auto& sp = slotParams_[size_t(slot)];
        sp.mode = apvts_.getRawParameterValue(p + "_mode");
        sp.position = apvts_.getRawParameterValue(p + "_position");
        sp.grainsize = apvts_.getRawParameterValue(p + "_grainsize");
        sp.density = apvts_.getRawParameterValue(p + "_density");
        sp.spray = apvts_.getRawParameterValue(p + "_spray");
        sp.stretch = apvts_.getRawParameterValue(p + "_stretch");
    }

    synth_.addSound(new SynthSound());
    for (int i = 0; i < kNumVoices; ++i)
        synth_.addVoice(new SynthVoice(wavetable_));
    rebindVoiceSources();
}

void SoundXAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    synth_.setCurrentPlaybackSampleRate(sampleRate);
    for (int i = 0; i < synth_.getNumVoices(); ++i)
        if (auto* v = dynamic_cast<SynthVoice*>(synth_.getVoice(i)))
            v->prepare(sampleRate, samplesPerBlock);
    rebindVoiceSources();
}

void SoundXAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    const float a = attack_->load(), d = decay_->load(), s = sustain_->load(),
                r = release_->load(), morph = morph_->load();
    for (int i = 0; i < synth_.getNumVoices(); ++i)
        if (auto* v = dynamic_cast<SynthVoice*>(synth_.getVoice(i))) {
            v->setMorph(morph);
            for (int slot = 0; slot < kNumSlots; ++slot) {
                const auto& sp = slotParams_[size_t(slot)];
                v->setSlotParams(slot, SynthVoice::Mode(int(sp.mode->load() + 0.5f)),
                                 sp.position->load(), sp.grainsize->load(),
                                 sp.density->load(), sp.spray->load(), sp.stretch->load());
            }
            v->setSharedParams(a, d, s, r);
        }

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

void SoundXAudioProcessor::rebindVoiceSources() {
    for (int i = 0; i < synth_.getNumVoices(); ++i)
        if (auto* v = dynamic_cast<SynthVoice*>(synth_.getVoice(i)))
            for (int slot = 0; slot < kNumSlots; ++slot) {
                const auto& assets = slots_[size_t(slot)];
                const auto* wt = assets.wavetable != nullptr ? assets.wavetable.get() : &wavetable_;
                v->setSlotSources(slot, wt, assets.sample.get(), assets.model.get());
            }
}

void SoundXAudioProcessor::applySample(int slot,
                                       std::shared_ptr<const soundx::engine::SampleData> sample,
                                       const juce::String& name) {
    auto imported = std::make_unique<soundx::engine::Wavetable>(
        soundx::engine::makeWavetableFromSample(*sample));
    auto model = std::make_unique<soundx::engine::SpectralModel>(
        soundx::engine::analyzeSpectral(*sample));

    suspendProcessing(true);
    auto& assets = slots_[size_t(slot)];
    assets.sample = std::move(sample);
    assets.wavetable = std::move(imported);
    assets.model = std::move(model);
    assets.name = name;
    rebindVoiceSources();
    suspendProcessing(false);
}

void SoundXAudioProcessor::loadSampleFile(int slot, const juce::File& file) {
    importPool_.addJob([this, slot, file] {
        juce::AudioFormatManager formats;
        formats.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
        if (reader == nullptr)
            return; // unsupported/corrupt: silently keep the current sample

        constexpr juce::int64 kMaxSeconds = 30;
        const auto numSamples = juce::jmin(reader->lengthInSamples,
                                           juce::int64(reader->sampleRate) * kMaxSeconds);
        if (numSamples < 2)
            return;

        juce::AudioBuffer<float> buffer(int(reader->numChannels), int(numSamples));
        reader->read(&buffer, 0, int(numSamples), 0, true, true);

        auto data = std::make_shared<soundx::engine::SampleData>();
        data->sourceSampleRate = reader->sampleRate;
        data->samples.resize(size_t(numSamples), 0.0f);
        const float channelScale = 1.0f / float(buffer.getNumChannels());
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch) {
            const float* in = buffer.getReadPointer(ch);
            for (int i = 0; i < int(numSamples); ++i)
                data->samples[size_t(i)] += in[i] * channelScale;
        }
        float peak = 0.0f;
        for (float v : data->samples)
            peak = std::max(peak, std::abs(v));
        if (peak > 1.0f)
            for (auto& v : data->samples)
                v /= peak;

        juce::MessageManager::callAsync([this, slot, data, name = file.getFileName()] {
            applySample(slot, data, name);
        });
    });
}

// JUCE plugin entry point
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new SoundXAudioProcessor();
}
