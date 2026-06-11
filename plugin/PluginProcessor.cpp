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
    layout.add(std::make_unique<P>(juce::ParameterID{"position", 1}, "Position",
                                   juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"mode", 1}, "Engine Mode",
        juce::StringArray{"Wavetable", "Granular", "Spectral"}, 0));
    layout.add(std::make_unique<P>(juce::ParameterID{"grainsize", 1}, "Grain Size",
                                   juce::NormalisableRange<float>(5.0f, 500.0f, 0.0f, 0.4f), 100.0f));
    layout.add(std::make_unique<P>(juce::ParameterID{"density", 1}, "Density",
                                   juce::NormalisableRange<float>(0.5f, 100.0f, 0.0f, 0.4f), 30.0f));
    layout.add(std::make_unique<P>(juce::ParameterID{"spray", 1}, "Spray",
                                   juce::NormalisableRange<float>(0.0f, 1.0f), 0.2f));
    {
        juce::NormalisableRange<float> stretchRange(0.0f, 4.0f);
        stretchRange.setSkewForCentre(1.0f);
        layout.add(std::make_unique<P>(juce::ParameterID{"stretch", 1}, "Stretch",
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
    position_ = apvts_.getRawParameterValue("position");
    mode_ = apvts_.getRawParameterValue("mode");
    grainsize_ = apvts_.getRawParameterValue("grainsize");
    density_ = apvts_.getRawParameterValue("density");
    spray_ = apvts_.getRawParameterValue("spray");
    stretch_ = apvts_.getRawParameterValue("stretch");

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
                r = release_->load(), pos = position_->load();
    const float gsize = grainsize_->load(), dens = density_->load(), spr = spray_->load();
    const float stretch = stretch_->load();
    const auto mode = SynthVoice::Mode(int(mode_->load() + 0.5f));
    for (int i = 0; i < synth_.getNumVoices(); ++i)
        if (auto* v = dynamic_cast<SynthVoice*>(synth_.getVoice(i))) {
            v->setMode(mode);
            v->setParams(a, d, s, r, pos, gsize, dens, spr, stretch);
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
    const auto* wt = importedWavetable_ != nullptr ? importedWavetable_.get() : &wavetable_;
    for (int i = 0; i < synth_.getNumVoices(); ++i)
        if (auto* v = dynamic_cast<SynthVoice*>(synth_.getVoice(i)))
            v->setSources(wt, sample_.get(), spectralModel_.get());
}

void SoundXAudioProcessor::applySample(std::shared_ptr<const soundx::engine::SampleData> sample,
                                       const juce::String& name) {
    auto imported = std::make_unique<soundx::engine::Wavetable>(
        soundx::engine::makeWavetableFromSample(*sample));
    auto model = std::make_unique<soundx::engine::SpectralModel>(
        soundx::engine::analyzeSpectral(*sample));

    suspendProcessing(true);
    sample_ = std::move(sample);
    importedWavetable_ = std::move(imported);
    spectralModel_ = std::move(model);
    sampleName_ = name;
    rebindVoiceSources();
    suspendProcessing(false);
}

void SoundXAudioProcessor::loadSampleFile(const juce::File& file) {
    importPool_.addJob([this, file] {
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

        juce::MessageManager::callAsync([this, data, name = file.getFileName()] {
            applySample(data, name);
        });
    });
}

// JUCE plugin entry point
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new SoundXAudioProcessor();
}
