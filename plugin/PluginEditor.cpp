#include "PluginEditor.h"

namespace {
constexpr auto kBackground = 0xff02090c;
constexpr auto kAccent = 0xff22d3ee;
constexpr auto kDim = 0xff0e3a40;

constexpr std::array<const char*, 6> kParamIds = {"gain", "attack", "decay",
                                                  "sustain", "release", "position"};
constexpr std::array<const char*, 6> kParamNames = {"GAIN", "ATK", "DEC",
                                                    "SUS", "REL", "POS"};
} // namespace

SoundXAudioProcessorEditor::SoundXAudioProcessorEditor(SoundXAudioProcessor& p)
    : AudioProcessorEditor(&p), processor_(p) {
    for (int i = 0; i < kNumSliders; ++i) {
        auto& s = sliders_[size_t(i)];
        s.slider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
        s.slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 64, 16);
        s.slider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(kAccent));
        s.slider.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(kDim));
        s.slider.setColour(juce::Slider::textBoxTextColourId, juce::Colour(kAccent));
        s.slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(kDim));
        // Let FL Studio's typing keyboard keep working while the editor is focused:
        // sliders grab keyboard focus by default and would swallow note keys.
        s.slider.setWantsKeyboardFocus(false);
        addAndMakeVisible(s.slider);

        s.label.setText(kParamNames[size_t(i)], juce::dontSendNotification);
        s.label.setJustificationType(juce::Justification::centred);
        s.label.setColour(juce::Label::textColourId, juce::Colour(kAccent));
        s.label.setFont(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 12.0f, juce::Font::plain));
        addAndMakeVisible(s.label);

        s.attachment = std::make_unique<Attachment>(processor_.apvts(), kParamIds[size_t(i)], s.slider);
    }
    setWantsKeyboardFocus(false);
    setResizable(true, true);
    setResizeLimits(540, 320, 1440, 850);
    setSize(720, 420);
}

void SoundXAudioProcessorEditor::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(kBackground));
    g.setColour(juce::Colour(kAccent));
    g.setFont(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 22.0f, juce::Font::plain));
    g.drawText("SOUNDX::ENGINE", getLocalBounds().removeFromTop(60), juce::Justification::centred);
    g.setColour(juce::Colour(kDim));
    g.drawRect(getLocalBounds().reduced(8), 1);
}

void SoundXAudioProcessorEditor::resized() {
    auto area = getLocalBounds().reduced(24);
    area.removeFromTop(60); // title
    const int cell = area.getWidth() / kNumSliders;
    for (auto& s : sliders_) {
        auto col = area.removeFromLeft(cell).reduced(8);
        s.label.setBounds(col.removeFromTop(18));
        s.slider.setBounds(col.withHeight(juce::jmin(col.getHeight(), 130)));
    }
}
