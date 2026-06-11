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
constexpr auto kAccentB = 0xfff9a8d4; // slot B identity color

constexpr std::array<const char*, 5> kSharedIds = {"gain", "attack", "decay", "sustain", "release"};
constexpr std::array<const char*, 5> kSharedNames = {"GAIN", "ATK", "DEC", "SUS", "REL"};
constexpr std::array<const char*, 5> kSlotIdSuffixes = {"_position", "_grainsize", "_density",
                                                        "_spray", "_stretch"};
constexpr std::array<const char*, 5> kSlotNames = {"POS", "GRAIN", "DENS", "SPRAY", "STRCH"};

bool isSupportedAudioFile(const juce::String& path) {
    return path.endsWithIgnoreCase(".wav") || path.endsWithIgnoreCase(".aif")
        || path.endsWithIgnoreCase(".aiff") || path.endsWithIgnoreCase(".flac")
        || path.endsWithIgnoreCase(".ogg") || path.endsWithIgnoreCase(".mp3");
}

const char* slotPrefix(int slot) { return slot == 0 ? "a" : "b"; }
} // namespace

void SoundXAudioProcessorEditor::styleSlider(LabeledSlider& s, const char* paramId, const char* name) {
    s.slider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    s.slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 15);
    s.slider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(kAccent));
    s.slider.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(kDim));
    s.slider.setColour(juce::Slider::textBoxTextColourId, juce::Colour(kAccent));
    s.slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(kDim));
    // Let FL Studio's typing keyboard keep working while the editor is focused.
    s.slider.setWantsKeyboardFocus(false);
    addAndMakeVisible(s.slider);

    s.label.setText(name, juce::dontSendNotification);
    s.label.setJustificationType(juce::Justification::centred);
    s.label.setColour(juce::Label::textColourId, juce::Colour(kAccent));
    s.label.setFont(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 11.0f, juce::Font::plain));
    addAndMakeVisible(s.label);

    s.attachment = std::make_unique<SliderAttachment>(processor_.apvts(), paramId, s.slider);
}

SoundXAudioProcessorEditor::SoundXAudioProcessorEditor(SoundXAudioProcessor& p)
    : AudioProcessorEditor(&p), processor_(p) {
    for (int i = 0; i < kNumSharedSliders; ++i)
        styleSlider(shared_[size_t(i)], kSharedIds[size_t(i)], kSharedNames[size_t(i)]);

    for (int slot = 0; slot < SoundXAudioProcessor::kNumSlots; ++slot)
        for (int i = 0; i < kNumSlotSliders; ++i) {
            const auto id = juce::String(slotPrefix(slot)) + kSlotIdSuffixes[size_t(i)];
            styleSlider(slots_[size_t(slot)][size_t(i)], id.toRawUTF8(), kSlotNames[size_t(i)]);
            if (slot == 1) {
                auto& s = slots_[size_t(slot)][size_t(i)];
                s.slider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(kAccentB));
                s.label.setColour(juce::Label::textColourId, juce::Colour(kAccentB));
            }
        }

    styleSlider(morph_, "morph", "MORPH A<->B");
    morph_.slider.setSliderStyle(juce::Slider::LinearHorizontal);
    morph_.slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 16);

    for (int slot = 0; slot < SoundXAudioProcessor::kNumSlots; ++slot) {
        auto& box = modeBoxes_[size_t(slot)];
        box.addItem("WAVETABLE", 1);
        box.addItem("GRANULAR", 2);
        box.addItem("SPECTRAL", 3);
        box.setColour(juce::ComboBox::backgroundColourId, juce::Colour(kBackground));
        box.setColour(juce::ComboBox::textColourId,
                      juce::Colour(slot == 0 ? kAccent : kAccentB));
        box.setColour(juce::ComboBox::outlineColourId, juce::Colour(kDim));
        box.setColour(juce::ComboBox::arrowColourId, juce::Colour(kAccent));
        box.setWantsKeyboardFocus(false);
        addAndMakeVisible(box);
        modeAttachments_[size_t(slot)] = std::make_unique<ComboAttachment>(
            processor_.apvts(), juce::String(slotPrefix(slot)) + "_mode", box);
    }

    setWantsKeyboardFocus(false);
    addMouseListener(this, true);
    setResizable(true, true);
    setResizeLimits(820, 460, 1800, 1000);
    setSize(1020, 560);
}

void SoundXAudioProcessorEditor::mouseUp(const juce::MouseEvent& e) {
    // Hand OS keyboard focus back to the host so FL's typing keyboard survives
    // clicks in our UI. Text fields are exempt so value entry still works.
    if (dynamic_cast<juce::TextEditor*>(e.eventComponent) != nullptr
        || dynamic_cast<juce::Label*>(e.eventComponent) != nullptr)
        return;
#if JUCE_WINDOWS
    if (auto* peer = getPeer())
        if (HWND parent = ::GetParent(static_cast<HWND>(peer->getNativeHandle())))
            ::SetFocus(parent);
#endif
}

juce::Rectangle<int> SoundXAudioProcessorEditor::dropZone(int slot) const {
    auto zone = getLocalBounds().reduced(24);
    zone.removeFromTop(44 + 30);
    zone = zone.removeFromTop(26);
    const int half = zone.getWidth() / 2;
    return slot == 0 ? zone.removeFromLeft(half - 6) : zone.removeFromRight(half - 6);
}

bool SoundXAudioProcessorEditor::isInterestedInFileDrag(const juce::StringArray& files) {
    for (const auto& f : files)
        if (isSupportedAudioFile(f))
            return true;
    return false;
}

void SoundXAudioProcessorEditor::fileDragEnter(const juce::StringArray&, int x, int) {
    dragHoverSlot_ = x < getWidth() / 2 ? 0 : 1;
    repaint();
}

void SoundXAudioProcessorEditor::fileDragExit(const juce::StringArray&) {
    dragHoverSlot_ = -1;
    repaint();
}

void SoundXAudioProcessorEditor::filesDropped(const juce::StringArray& files, int x, int) {
    dragHoverSlot_ = -1;
    const int slot = x < getWidth() / 2 ? 0 : 1;
    for (const auto& f : files)
        if (isSupportedAudioFile(f)) {
            processor_.loadSampleFile(slot, juce::File(f));
            break;
        }
    repaint();
}

void SoundXAudioProcessorEditor::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(kBackground));
    g.setColour(juce::Colour(kAccent));
    g.setFont(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 22.0f, juce::Font::plain));
    g.drawText("SOUNDX::ENGINE", getLocalBounds().removeFromTop(44), juce::Justification::centred);

    g.setFont(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 11.0f, juce::Font::plain));
    for (int slot = 0; slot < SoundXAudioProcessor::kNumSlots; ++slot) {
        const auto zone = dropZone(slot);
        const auto idColor = juce::Colour(slot == 0 ? kAccent : kAccentB);
        const bool hover = dragHoverSlot_ == slot;
        g.setColour(hover ? idColor : juce::Colour(kDim));
        g.drawRect(zone, 1);
        const auto name = processor_.currentSampleName(slot);
        const auto text = juce::String(slot == 0 ? "[A] " : "[B] ")
                        + (name.isEmpty() ? juce::String("DROP AUDIO HERE") : name);
        g.setColour(idColor.withAlpha(hover ? 1.0f : 0.75f));
        g.drawText(text, zone, juce::Justification::centred);
    }

    g.setColour(juce::Colour(kDim));
    g.drawRect(getLocalBounds().reduced(8), 1);
}

void SoundXAudioProcessorEditor::resized() {
    auto area = getLocalBounds().reduced(24);
    area.removeFromTop(44); // title

    auto modeRow = area.removeFromTop(24);
    const int boxW = 150;
    modeBoxes_[0].setBounds(modeRow.removeFromLeft(boxW));
    modeBoxes_[1].setBounds(modeRow.removeFromRight(boxW));
    area.removeFromTop(6);
    area.removeFromTop(26); // drop zones (painted)
    area.removeFromTop(6);

    auto morphRow = area.removeFromTop(40);
    morph_.label.setBounds(morphRow.removeFromLeft(110));
    morph_.slider.setBounds(morphRow);
    area.removeFromTop(6);

    auto sharedRow = area.removeFromTop(area.getHeight() / 2);
    const int sharedCell = sharedRow.getWidth() / kNumSharedSliders;
    for (auto& s : shared_) {
        auto col = sharedRow.removeFromLeft(sharedCell).reduced(8);
        s.label.setBounds(col.removeFromTop(16));
        s.slider.setBounds(col.withHeight(juce::jmin(col.getHeight(), 110)));
    }

    auto slotRow = area;
    const int half = slotRow.getWidth() / 2;
    auto aArea = slotRow.removeFromLeft(half - 8);
    auto bArea = slotRow.withTrimmedLeft(16);
    for (int slot = 0; slot < SoundXAudioProcessor::kNumSlots; ++slot) {
        auto row = slot == 0 ? aArea : bArea;
        const int cell = row.getWidth() / kNumSlotSliders;
        for (auto& s : slots_[size_t(slot)]) {
            auto col = row.removeFromLeft(cell).reduced(4);
            s.label.setBounds(col.removeFromTop(16));
            s.slider.setBounds(col.withHeight(juce::jmin(col.getHeight(), 110)));
        }
    }
}
