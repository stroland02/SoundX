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

// Modulation destinations. Index 0 is "Off"; the rest line up with
// kDestRanges/kDestMins below.
const juce::StringArray kDestNames{"Off",     "Morph",   "Gain",    "A Pos",  "A Grain",
                                   "A Dens",  "A Spray", "A Strch", "B Pos",  "B Grain",
                                   "B Dens",  "B Spray", "B Strch"};
constexpr int kNumDests = 13;
// natural-unit span and minimum of each destination (index 0 unused)
constexpr std::array<float, kNumDests> kDestRanges = {0.0f, 1.0f, 1.0f, 1.0f, 495.0f, 99.5f, 1.0f,
                                                      4.0f, 1.0f, 495.0f, 99.5f, 1.0f, 4.0f};
constexpr std::array<float, kNumDests> kDestMins = {0.0f, 0.0f, 0.0f, 0.0f, 5.0f, 0.5f, 0.0f,
                                                    0.0f, 0.0f, 5.0f, 0.5f, 0.0f, 0.0f};
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

    for (int i = 1; i <= kNumLfos; ++i) {
        const juce::String p = "lfo" + juce::String(i);
        juce::NormalisableRange<float> rateRange(0.01f, 20.0f);
        rateRange.setSkewForCentre(2.0f);
        layout.add(std::make_unique<P>(juce::ParameterID{p + "_rate", 1}, p.toUpperCase() + " Rate",
                                       rateRange, 1.0f));
        layout.add(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID{p + "_shape", 1}, p.toUpperCase() + " Shape",
            juce::StringArray{"Sine", "Triangle", "Saw", "Square", "S&H"}, 0));
        layout.add(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID{p + "_dest", 1}, p.toUpperCase() + " Destination", kDestNames, 0));
        layout.add(std::make_unique<P>(juce::ParameterID{p + "_amount", 1}, p.toUpperCase() + " Amount",
                                       juce::NormalisableRange<float>(-1.0f, 1.0f), 0.0f));
    }
    for (int m = 1; m <= kNumMacros; ++m) {
        const juce::String p = "macro" + juce::String(m);
        layout.add(std::make_unique<P>(juce::ParameterID{p, 1}, p.toUpperCase(),
                                       juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));
        layout.add(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID{p + "_dest", 1}, p.toUpperCase() + " Destination", kDestNames, 0));
        layout.add(std::make_unique<P>(juce::ParameterID{p + "_amount", 1}, p.toUpperCase() + " Amount",
                                       juce::NormalisableRange<float>(-1.0f, 1.0f), 0.0f));
    }

    using B = juce::AudioParameterBool;
    layout.add(std::make_unique<B>(juce::ParameterID{"dist_on", 1}, "Distortion On", false));
    layout.add(std::make_unique<P>(juce::ParameterID{"dist_drive", 1}, "Distortion Drive",
                                   juce::NormalisableRange<float>(1.0f, 20.0f, 0.0f, 0.5f), 4.0f));
    layout.add(std::make_unique<P>(juce::ParameterID{"dist_mix", 1}, "Distortion Mix",
                                   juce::NormalisableRange<float>(0.0f, 1.0f), 1.0f));
    layout.add(std::make_unique<B>(juce::ParameterID{"comp_on", 1}, "OTT On", false));
    layout.add(std::make_unique<P>(juce::ParameterID{"comp_depth", 1}, "OTT Depth",
                                   juce::NormalisableRange<float>(0.0f, 1.0f), 0.5f));
    layout.add(std::make_unique<B>(juce::ParameterID{"chorus_on", 1}, "Chorus On", false));
    layout.add(std::make_unique<P>(juce::ParameterID{"chorus_rate", 1}, "Chorus Rate",
                                   juce::NormalisableRange<float>(0.1f, 5.0f, 0.0f, 0.5f), 1.0f));
    layout.add(std::make_unique<P>(juce::ParameterID{"chorus_depth", 1}, "Chorus Depth",
                                   juce::NormalisableRange<float>(1.0f, 15.0f), 8.0f));
    layout.add(std::make_unique<P>(juce::ParameterID{"chorus_mix", 1}, "Chorus Mix",
                                   juce::NormalisableRange<float>(0.0f, 1.0f), 0.5f));
    layout.add(std::make_unique<B>(juce::ParameterID{"delay_on", 1}, "Delay On", false));
    layout.add(std::make_unique<P>(juce::ParameterID{"delay_time", 1}, "Delay Time",
                                   juce::NormalisableRange<float>(10.0f, 2000.0f, 0.0f, 0.4f), 350.0f));
    layout.add(std::make_unique<P>(juce::ParameterID{"delay_feedback", 1}, "Delay Feedback",
                                   juce::NormalisableRange<float>(0.0f, 0.95f), 0.35f));
    layout.add(std::make_unique<P>(juce::ParameterID{"delay_mix", 1}, "Delay Mix",
                                   juce::NormalisableRange<float>(0.0f, 1.0f), 0.3f));
    layout.add(std::make_unique<B>(juce::ParameterID{"reverb_on", 1}, "Reverb On", false));
    layout.add(std::make_unique<P>(juce::ParameterID{"reverb_size", 1}, "Reverb Size",
                                   juce::NormalisableRange<float>(0.0f, 1.0f), 0.5f));
    layout.add(std::make_unique<P>(juce::ParameterID{"reverb_damp", 1}, "Reverb Damp",
                                   juce::NormalisableRange<float>(0.0f, 1.0f), 0.5f));
    layout.add(std::make_unique<P>(juce::ParameterID{"reverb_mix", 1}, "Reverb Mix",
                                   juce::NormalisableRange<float>(0.0f, 1.0f), 0.3f));
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
    for (int i = 0; i < kNumLfos; ++i) {
        const juce::String p = "lfo" + juce::String(i + 1);
        auto& mp = lfoParams_[size_t(i)];
        mp.rate = apvts_.getRawParameterValue(p + "_rate");
        mp.shape = apvts_.getRawParameterValue(p + "_shape");
        mp.dest = apvts_.getRawParameterValue(p + "_dest");
        mp.amount = apvts_.getRawParameterValue(p + "_amount");
    }
    for (int m = 0; m < kNumMacros; ++m) {
        const juce::String p = "macro" + juce::String(m + 1);
        auto& mp = macroParams_[size_t(m)];
        mp.value = apvts_.getRawParameterValue(p);
        mp.dest = apvts_.getRawParameterValue(p + "_dest");
        mp.amount = apvts_.getRawParameterValue(p + "_amount");
    }

    distParams_ = {apvts_.getRawParameterValue("dist_on"), apvts_.getRawParameterValue("dist_drive"),
                   apvts_.getRawParameterValue("dist_mix"), nullptr};
    compParams_ = {apvts_.getRawParameterValue("comp_on"), apvts_.getRawParameterValue("comp_depth"),
                   nullptr, nullptr};
    chorusParams_ = {apvts_.getRawParameterValue("chorus_on"), apvts_.getRawParameterValue("chorus_rate"),
                     apvts_.getRawParameterValue("chorus_depth"), apvts_.getRawParameterValue("chorus_mix")};
    delayParams_ = {apvts_.getRawParameterValue("delay_on"), apvts_.getRawParameterValue("delay_time"),
                    apvts_.getRawParameterValue("delay_feedback"), apvts_.getRawParameterValue("delay_mix")};
    reverbParams_ = {apvts_.getRawParameterValue("reverb_on"), apvts_.getRawParameterValue("reverb_size"),
                     apvts_.getRawParameterValue("reverb_damp"), apvts_.getRawParameterValue("reverb_mix")};

    synth_.addSound(new SynthSound());
    for (int i = 0; i < kNumVoices; ++i)
        synth_.addVoice(new SynthVoice(wavetable_));
    rebindVoiceSources();
}

void SoundXAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    synth_.setCurrentPlaybackSampleRate(sampleRate);
    for (auto& lfo : lfos_)
        lfo.setSampleRate(sampleRate);
    distortion_.prepare(sampleRate, samplesPerBlock);
    comp_.prepare(sampleRate, samplesPerBlock);
    chorus_.prepare(sampleRate, samplesPerBlock);
    delay_.prepare(sampleRate, samplesPerBlock);
    reverb_.prepare(sampleRate, samplesPerBlock);
    for (int i = 0; i < synth_.getNumVoices(); ++i)
        if (auto* v = dynamic_cast<SynthVoice*>(synth_.getVoice(i)))
            v->prepare(sampleRate, samplesPerBlock);
    rebindVoiceSources();
}

void SoundXAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    // --- control-rate modulation: accumulate per-destination offsets ---
    std::array<float, kNumDests> offsets{};
    for (int i = 0; i < kNumLfos; ++i) {
        auto& mp = lfoParams_[size_t(i)];
        auto& lfo = lfos_[size_t(i)];
        lfo.setRate(mp.rate->load());
        lfo.setShape(soundx::engine::Lfo::Shape(int(mp.shape->load() + 0.5f)));
        const float value = lfo.advance(buffer.getNumSamples());
        const int dest = int(mp.dest->load() + 0.5f);
        if (dest > 0 && dest < kNumDests)
            offsets[size_t(dest)] += value * mp.amount->load() * kDestRanges[size_t(dest)];
    }
    for (int m = 0; m < kNumMacros; ++m) {
        auto& mp = macroParams_[size_t(m)];
        const int dest = int(mp.dest->load() + 0.5f);
        if (dest > 0 && dest < kNumDests)
            offsets[size_t(dest)] += mp.value->load() * mp.amount->load() * kDestRanges[size_t(dest)];
    }
    auto modulated = [&offsets](float base, int dest) {
        const float lo = kDestMins[size_t(dest)];
        return juce::jlimit(lo, lo + kDestRanges[size_t(dest)], base + offsets[size_t(dest)]);
    };

    const float a = attack_->load(), d = decay_->load(), s = sustain_->load(),
                r = release_->load();
    const float morph = modulated(morph_->load(), 1);
    const float gain = modulated(gain_->load(), 2);
    for (int i = 0; i < synth_.getNumVoices(); ++i)
        if (auto* v = dynamic_cast<SynthVoice*>(synth_.getVoice(i))) {
            v->setMorph(morph);
            for (int slot = 0; slot < kNumSlots; ++slot) {
                const auto& sp = slotParams_[size_t(slot)];
                const int base = 3 + slot * 5; // dest indices: pos, grain, dens, spray, stretch
                v->setSlotParams(slot, SynthVoice::Mode(int(sp.mode->load() + 0.5f)),
                                 modulated(sp.position->load(), base),
                                 modulated(sp.grainsize->load(), base + 1),
                                 modulated(sp.density->load(), base + 2),
                                 modulated(sp.spray->load(), base + 3),
                                 modulated(sp.stretch->load(), base + 4));
            }
            v->setSharedParams(a, d, s, r);
        }

    synth_.renderNextBlock(buffer, midi, 0, buffer.getNumSamples());
    buffer.applyGain(gain);

    // master FX chain (fixed order): dist -> OTT -> chorus -> delay -> reverb
    if (buffer.getNumChannels() >= 1) {
        float* l = buffer.getWritePointer(0);
        float* r = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : l;
        const int n = buffer.getNumSamples();
        if (distParams_.on->load() >= 0.5f) {
            distortion_.setParams(distParams_.p1->load(), distParams_.p2->load());
            distortion_.process(l, r, n);
        }
        if (compParams_.on->load() >= 0.5f) {
            comp_.setParams(compParams_.p1->load());
            comp_.process(l, r, n);
        }
        if (chorusParams_.on->load() >= 0.5f) {
            chorus_.setParams(chorusParams_.p1->load(), chorusParams_.p2->load(),
                              chorusParams_.p3->load());
            chorus_.process(l, r, n);
        }
        if (delayParams_.on->load() >= 0.5f) {
            delay_.setParams(delayParams_.p1->load(), delayParams_.p2->load(),
                             delayParams_.p3->load());
            delay_.process(l, r, n);
        }
        if (reverbParams_.on->load() >= 0.5f) {
            reverb_.setParams(reverbParams_.p1->load(), reverbParams_.p2->load(),
                              reverbParams_.p3->load());
            reverb_.process(l, r, n);
        }
    }
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
