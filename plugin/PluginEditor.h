#pragma once
#include <array>
#include "PluginProcessor.h"

class SoundXAudioProcessorEditor : public juce::AudioProcessorEditor,
                                   public juce::FileDragAndDropTarget {
public:
    explicit SoundXAudioProcessorEditor(SoundXAudioProcessor&);
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseUp(const juce::MouseEvent&) override;

    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;
    void fileDragEnter(const juce::StringArray&, int, int) override;
    void fileDragExit(const juce::StringArray&) override;

private:
    static constexpr int kNumSliders = 9;
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    struct LabeledSlider {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<SliderAttachment> attachment;
    };

    SoundXAudioProcessor& processor_;
    std::array<LabeledSlider, kNumSliders> sliders_;
    juce::ComboBox modeBox_;
    std::unique_ptr<ComboAttachment> modeAttachment_;
    bool dragHover_ = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SoundXAudioProcessorEditor)
};
