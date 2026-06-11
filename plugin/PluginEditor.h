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
    static constexpr int kNumSharedSliders = 5; // gain + ADSR
    static constexpr int kNumSlotSliders = 5;   // position, grainsize, density, spray, stretch

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    struct LabeledSlider {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<SliderAttachment> attachment;
    };

    void styleSlider(LabeledSlider& s, const char* paramId, const char* name);
    juce::Rectangle<int> dropZone(int slot) const;

    SoundXAudioProcessor& processor_;
    std::array<LabeledSlider, kNumSharedSliders> shared_;
    std::array<std::array<LabeledSlider, kNumSlotSliders>, SoundXAudioProcessor::kNumSlots> slots_;
    LabeledSlider morph_;
    std::array<juce::ComboBox, SoundXAudioProcessor::kNumSlots> modeBoxes_;
    std::array<std::unique_ptr<ComboAttachment>, SoundXAudioProcessor::kNumSlots> modeAttachments_;
    int dragHoverSlot_ = -1; // -1 = none

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SoundXAudioProcessorEditor)
};
