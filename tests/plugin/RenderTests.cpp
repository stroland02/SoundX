#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <numbers>
#include "PluginProcessor.h"
#include "engine/SampleImporter.h"

namespace {
struct BlockResult {
    float peakRms = 0.0f;
    bool allFinite = true;
};

BlockResult renderBlocks(SoundXAudioProcessor& proc, juce::MidiBuffer& midi, int numBlocks, int blockSize) {
    BlockResult r;
    juce::AudioBuffer<float> buffer(2, blockSize);
    for (int b = 0; b < numBlocks; ++b) {
        buffer.clear();
        proc.processBlock(buffer, midi);
        midi.clear();
        r.peakRms = std::max(r.peakRms, buffer.getRMSLevel(0, 0, blockSize));
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            for (int i = 0; i < blockSize; ++i)
                if (!std::isfinite(buffer.getSample(ch, i)))
                    r.allFinite = false;
    }
    return r;
}

std::shared_ptr<soundx::engine::SampleData> sineSampleData(double hz = 440.0) {
    auto sample = std::make_shared<soundx::engine::SampleData>();
    sample->sourceSampleRate = 44100.0;
    sample->samples.resize(44100);
    for (std::size_t i = 0; i < sample->samples.size(); ++i)
        sample->samples[i] = float(std::sin(2.0 * std::numbers::pi * hz * double(i) / 44100.0));
    return sample;
}

void setChoice(SoundXAudioProcessor& proc, const char* id, float normalized) {
    auto* p = proc.apvts().getParameter(id);
    REQUIRE(p != nullptr);
    p->setValueNotifyingHost(normalized);
}
} // namespace

TEST_CASE("held note produces finite, audible output; release returns to silence") {
    juce::ScopedJuceInitialiser_GUI juceInit;
    SoundXAudioProcessor proc;
    proc.prepareToPlay(44100.0, 512);

    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.8f), 0);
    auto held = renderBlocks(proc, midi, 40, 512); // ~460ms
    REQUIRE(held.allFinite);
    REQUIRE(held.peakRms > 0.01f);

    midi.addEvent(juce::MidiMessage::noteOff(1, 60), 0);
    renderBlocks(proc, midi, 80, 512); // ~930ms >> max release tail at defaults
    juce::MidiBuffer empty;
    auto after = renderBlocks(proc, empty, 4, 512);
    REQUIRE(after.peakRms < 1.0e-4f);
}

TEST_CASE("state save and restore round-trips a parameter") {
    juce::ScopedJuceInitialiser_GUI juceInit;
    SoundXAudioProcessor proc;
    auto* gain = proc.apvts().getParameter("gain");
    REQUIRE(gain != nullptr);
    gain->setValueNotifyingHost(0.25f);

    juce::MemoryBlock state;
    proc.getStateInformation(state);

    gain->setValueNotifyingHost(0.9f);
    proc.setStateInformation(state.getData(), int(state.getSize()));
    REQUIRE(std::abs(gain->getValue() - 0.25f) < 1.0e-4f);
}

TEST_CASE("slot A granular plays a programmatically loaded sample") {
    juce::ScopedJuceInitialiser_GUI juceInit;
    SoundXAudioProcessor proc;
    proc.prepareToPlay(44100.0, 512);
    proc.applySample(0, sineSampleData(), "test-sine");
    setChoice(proc, "a_mode", 0.5f); // Granular

    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.9f), 0);
    auto held = renderBlocks(proc, midi, 40, 512);
    REQUIRE(held.allFinite);
    REQUIRE(held.peakRms > 0.01f);
}

TEST_CASE("morph 1 plays slot B; empty slot B granular is silent") {
    juce::ScopedJuceInitialiser_GUI juceInit;

    SECTION("slot B granular with a sample is audible at morph 1") {
        SoundXAudioProcessor proc;
        proc.prepareToPlay(44100.0, 512);
        proc.applySample(1, sineSampleData(), "b-sine");
        setChoice(proc, "b_mode", 0.5f); // Granular
        proc.apvts().getParameter("morph")->setValueNotifyingHost(1.0f);

        juce::MidiBuffer midi;
        midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.9f), 0);
        auto held = renderBlocks(proc, midi, 40, 512);
        REQUIRE(held.allFinite);
        REQUIRE(held.peakRms > 0.01f);
    }

    SECTION("slot B granular without a sample is silent at morph 1") {
        SoundXAudioProcessor proc;
        proc.prepareToPlay(44100.0, 512);
        setChoice(proc, "b_mode", 0.5f); // Granular, no sample loaded
        proc.apvts().getParameter("morph")->setValueNotifyingHost(1.0f);

        juce::MidiBuffer midi;
        midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.9f), 0);
        auto held = renderBlocks(proc, midi, 10, 512);
        REQUIRE(held.allFinite);
        REQUIRE(held.peakRms < 1.0e-4f);
    }
}

TEST_CASE("macro routed to morph deterministically silences slot A") {
    juce::ScopedJuceInitialiser_GUI juceInit;
    SoundXAudioProcessor proc;
    proc.prepareToPlay(44100.0, 512);
    // slot B = granular with no sample (silent); macro1 -> morph, amount +1
    setChoice(proc, "b_mode", 0.5f);
    setChoice(proc, "macro1_dest", 1.0f / 12.0f); // index 1 of 13 = Morph
    proc.apvts().getParameter("macro1_amount")->setValueNotifyingHost(1.0f); // +1

    auto play = [&] {
        juce::MidiBuffer midi;
        midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.9f), 0);
        auto held = renderBlocks(proc, midi, 20, 512);
        midi.addEvent(juce::MidiMessage::noteOff(1, 60), 0);
        renderBlocks(proc, midi, 80, 512); // flush the tail
        return held;
    };

    proc.apvts().getParameter("macro1")->setValueNotifyingHost(0.0f);
    auto atZero = play();
    REQUIRE(atZero.allFinite);
    REQUIRE(atZero.peakRms > 0.01f); // morph stays 0: slot A wavetable audible

    proc.apvts().getParameter("macro1")->setValueNotifyingHost(1.0f);
    auto atOne = play();
    REQUIRE(atOne.allFinite);
    REQUIRE(atOne.peakRms < 1.0e-4f); // morph pushed to 1: empty slot B = silence
}

TEST_CASE("mod source with amount 0 changes nothing") {
    juce::ScopedJuceInitialiser_GUI juceInit;
    SoundXAudioProcessor proc;
    proc.prepareToPlay(44100.0, 512);
    setChoice(proc, "lfo1_dest", 1.0f / 12.0f); // Morph
    proc.apvts().getParameter("lfo1_rate")->setValueNotifyingHost(1.0f); // fast
    // amount left at default 0

    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.9f), 0);
    auto held = renderBlocks(proc, midi, 20, 512);
    REQUIRE(held.allFinite);
    REQUIRE(held.peakRms > 0.01f); // unmodulated slot A still plays
}

TEST_CASE("spectral-to-spectral mid-morph is audible and finite") {
    juce::ScopedJuceInitialiser_GUI juceInit;
    SoundXAudioProcessor proc;
    proc.prepareToPlay(44100.0, 512);
    proc.applySample(0, sineSampleData(440.0), "a-440");
    proc.applySample(1, sineSampleData(660.0), "b-660");
    setChoice(proc, "a_mode", 1.0f); // Spectral
    setChoice(proc, "b_mode", 1.0f); // Spectral
    proc.apvts().getParameter("morph")->setValueNotifyingHost(0.5f);

    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.9f), 0);
    auto held = renderBlocks(proc, midi, 40, 512);
    REQUIRE(held.allFinite);
    REQUIRE(held.peakRms > 0.01f);
}
