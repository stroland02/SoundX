#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "BinaryData.h"

// Lists factory presets (embedded via BinaryData) and user presets
// (Documents/SoundX/Presets/*.soundxpreset); applies them to the APVTS.
class PresetManager {
public:
    explicit PresetManager(juce::AudioProcessorValueTreeState& apvts) : apvts_(apvts) {
        refresh();
    }

    void refresh() {
        names_.clear();
        factoryCount_ = 0;
        for (int i = 0; i < BinaryData::namedResourceListSize; ++i) {
            const juce::String original(BinaryData::originalFilenames[i]);
            if (original.endsWith(".soundxpreset")) {
                names_.add(prettyName(original));
                resourceIndices_.add(i);
                ++factoryCount_;
            }
        }
        userFiles_ = userPresetDirectory().findChildFiles(juce::File::findFiles, false,
                                                          "*.soundxpreset");
        userFiles_.sort();
        for (const auto& f : userFiles_)
            names_.add("[user] " + prettyName(f.getFileName()));
    }

    const juce::StringArray& presetNames() const { return names_; }

    void applyPreset(int index) {
        std::unique_ptr<juce::XmlElement> xml;
        if (index < 0 || index >= names_.size())
            return;
        if (index < factoryCount_) {
            int dataSize = 0;
            const auto resource = resourceIndices_[index];
            const char* data = BinaryData::getNamedResource(
                BinaryData::namedResourceList[resource], dataSize);
            if (data != nullptr)
                xml = juce::parseXML(juce::String::fromUTF8(data, dataSize));
        } else {
            xml = juce::parseXML(userFiles_[index - factoryCount_]);
        }
        if (xml != nullptr && xml->hasTagName(apvts_.state.getType()))
            apvts_.replaceState(juce::ValueTree::fromXml(*xml));
    }

    // Saves the current state as a timestamped user preset; returns its name.
    juce::String saveUserPreset() {
        const auto dir = userPresetDirectory();
        dir.createDirectory();
        const auto stamp = juce::Time::getCurrentTime().formatted("%Y%m%d-%H%M%S");
        const auto file = dir.getChildFile("User-" + stamp + ".soundxpreset");
        if (auto xml = apvts_.copyState().createXml())
            file.replaceWithText(xml->toString());
        refresh();
        return file.getFileNameWithoutExtension();
    }

    static juce::File userPresetDirectory() {
        return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
            .getChildFile("SoundX")
            .getChildFile("Presets");
    }

private:
    static juce::String prettyName(const juce::String& filename) {
        return filename.upToLastOccurrenceOf(".soundxpreset", false, true)
            .replaceCharacter('_', ' ');
    }

    juce::AudioProcessorValueTreeState& apvts_;
    juce::StringArray names_;
    juce::Array<int> resourceIndices_;
    juce::Array<juce::File> userFiles_;
    int factoryCount_ = 0;
};
