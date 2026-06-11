#pragma once
#include <array>
#include "PluginProcessor.h"

class SoundXAudioProcessorEditor : public juce::AudioProcessorEditor {
public:
    explicit SoundXAudioProcessorEditor(SoundXAudioProcessor&);
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseUp(const juce::MouseEvent&) override;

private:
    static constexpr int kNumSliders = 6;
    using Attachment = juce::AudioProcessorValueTreeState::SliderAttachment;

    struct LabeledSlider {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<Attachment> attachment;
    };

    SoundXAudioProcessor& processor_;
    std::array<LabeledSlider, kNumSliders> sliders_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SoundXAudioProcessorEditor)
};
