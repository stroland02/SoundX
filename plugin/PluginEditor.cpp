#include "PluginEditor.h"

#if JUCE_WINDOWS
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
 #endif
 #include <windows.h>
#endif

namespace {
constexpr auto kBackground = 0xff02090c;
constexpr auto kAccent = 0xff22d3ee;
constexpr auto kDim = 0xff0e3a40;

constexpr std::array<const char*, 10> kParamIds = {"gain", "attack", "decay", "sustain",
                                                   "release", "position", "grainsize",
                                                   "density", "spray", "stretch"};
constexpr std::array<const char*, 10> kParamNames = {"GAIN", "ATK", "DEC", "SUS", "REL",
                                                     "POS", "GRAIN", "DENS", "SPRAY", "STRCH"};

bool isSupportedAudioFile(const juce::String& path) {
    return path.endsWithIgnoreCase(".wav") || path.endsWithIgnoreCase(".aif")
        || path.endsWithIgnoreCase(".aiff") || path.endsWithIgnoreCase(".flac")
        || path.endsWithIgnoreCase(".ogg") || path.endsWithIgnoreCase(".mp3");
}
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

        s.attachment = std::make_unique<SliderAttachment>(processor_.apvts(), kParamIds[size_t(i)], s.slider);
    }

    modeBox_.addItem("WAVETABLE", 1);
    modeBox_.addItem("GRANULAR", 2);
    modeBox_.addItem("SPECTRAL", 3);
    modeBox_.setColour(juce::ComboBox::backgroundColourId, juce::Colour(kBackground));
    modeBox_.setColour(juce::ComboBox::textColourId, juce::Colour(kAccent));
    modeBox_.setColour(juce::ComboBox::outlineColourId, juce::Colour(kDim));
    modeBox_.setColour(juce::ComboBox::arrowColourId, juce::Colour(kAccent));
    modeBox_.setWantsKeyboardFocus(false);
    addAndMakeVisible(modeBox_);
    modeAttachment_ = std::make_unique<ComboAttachment>(processor_.apvts(), "mode", modeBox_);

    setWantsKeyboardFocus(false);
    // Hear about clicks on every child so we can hand keyboard focus back to the host.
    addMouseListener(this, true);
    setResizable(true, true);
    setResizeLimits(640, 360, 1600, 900);
    setSize(860, 460);
}

void SoundXAudioProcessorEditor::mouseUp(const juce::MouseEvent& e) {
    // Keep FL Studio's typing keyboard alive: clicking our UI moves OS keyboard
    // focus onto the plugin window, so hand it straight back to the host's
    // wrapper window. Skip text fields so typing values into knobs still works.
    if (dynamic_cast<juce::TextEditor*>(e.eventComponent) != nullptr
        || dynamic_cast<juce::Label*>(e.eventComponent) != nullptr)
        return;
#if JUCE_WINDOWS
    if (auto* peer = getPeer())
        if (HWND parent = ::GetParent(static_cast<HWND>(peer->getNativeHandle())))
            ::SetFocus(parent);
#endif
}

bool SoundXAudioProcessorEditor::isInterestedInFileDrag(const juce::StringArray& files) {
    for (const auto& f : files)
        if (isSupportedAudioFile(f))
            return true;
    return false;
}

void SoundXAudioProcessorEditor::fileDragEnter(const juce::StringArray&, int, int) {
    dragHover_ = true;
    repaint();
}

void SoundXAudioProcessorEditor::fileDragExit(const juce::StringArray&) {
    dragHover_ = false;
    repaint();
}

void SoundXAudioProcessorEditor::filesDropped(const juce::StringArray& files, int, int) {
    dragHover_ = false;
    for (const auto& f : files)
        if (isSupportedAudioFile(f)) {
            processor_.loadSampleFile(juce::File(f));
            break;
        }
    repaint();
}

void SoundXAudioProcessorEditor::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(kBackground));
    g.setColour(juce::Colour(kAccent));
    g.setFont(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 22.0f, juce::Font::plain));
    g.drawText("SOUNDX::ENGINE", getLocalBounds().removeFromTop(48), juce::Justification::centred);

    g.setFont(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 12.0f, juce::Font::plain));
    const auto sampleName = processor_.currentSampleName();
    const auto sampleText = sampleName.isEmpty()
        ? juce::String("DRAG AUDIO FILE HERE - plays in both engines")
        : "SAMPLE: " + sampleName;
    g.setColour(juce::Colour(dragHover_ ? kAccent : kDim).brighter(dragHover_ ? 0.4f : 0.0f));
    auto sampleZone = getLocalBounds().reduced(24).removeFromTop(72).removeFromBottom(24);
    g.drawRect(sampleZone, 1);
    g.setColour(juce::Colour(kAccent).withAlpha(dragHover_ ? 1.0f : 0.7f));
    g.drawText(sampleText, sampleZone, juce::Justification::centred);

    g.setColour(juce::Colour(kDim));
    g.drawRect(getLocalBounds().reduced(8), 1);
}

void SoundXAudioProcessorEditor::resized() {
    auto area = getLocalBounds().reduced(24);
    area.removeFromTop(48); // title
    auto topRow = area.removeFromTop(24);
    modeBox_.setBounds(topRow.removeFromLeft(160));
    area.removeFromTop(28); // sample drop zone (painted)

    const int cell = area.getWidth() / kNumSliders;
    for (auto& s : sliders_) {
        auto col = area.removeFromLeft(cell).reduced(6);
        s.label.setBounds(col.removeFromTop(18));
        s.slider.setBounds(col.withHeight(juce::jmin(col.getHeight(), 130)));
    }
}
