#include "PluginEditor.h"

SoundXAudioProcessorEditor::SoundXAudioProcessorEditor(SoundXAudioProcessor& p)
    : AudioProcessorEditor(&p) {
    setSize(720, 420);
}

void SoundXAudioProcessorEditor::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xff02090c));
    g.setColour(juce::Colour(0xff22d3ee));
    g.setFont(juce::FontOptions(20.0f));
    g.drawText("SOUNDX::ENGINE", getLocalBounds(), juce::Justification::centred);
}
