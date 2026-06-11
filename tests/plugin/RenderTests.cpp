#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include "PluginProcessor.h"

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
