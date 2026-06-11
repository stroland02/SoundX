juce_add_plugin(SoundX
    COMPANY_NAME "SoundX Audio"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Sndx
    PLUGIN_CODE Sx01
    FORMATS VST3 Standalone
    PRODUCT_NAME "SoundX")

target_sources(SoundX PRIVATE
    PluginProcessor.cpp
    PluginEditor.cpp
    OrbitView.cpp)

target_include_directories(SoundX PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_definitions(SoundX PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(SoundX
    PUBLIC
        juce::juce_audio_utils
    PRIVATE
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
