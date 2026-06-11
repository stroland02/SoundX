#pragma once
#include <array>
#include <vector>
#include "PluginProcessor.h"

// Harmonic Orbits: real-time 3D-projected orbital view of the output's
// partials. Pulls samples from the processor's lock-free tap on a 30 Hz
// timer, FFTs them on the UI thread, and animates one body per partial.
// The MORPH parameter rolls the projection plane (the "4D twist").
class OrbitView : public juce::Component, private juce::Timer {
public:
    explicit OrbitView(SoundXAudioProcessor&);
    ~OrbitView() override;

    void paint(juce::Graphics&) override;

private:
    static constexpr int kFftSize = 2048;
    static constexpr int kMaxBodies = 16;

    struct Body {
        float ratio = 1.0f;     // frequency ratio to the fundamental
        float amp = 0.0f;
        float angle = 0.0f;     // orbital angle, radians
        float inharmonicity = 0.0f; // distance from nearest integer ratio, 0..0.5
        bool active = false;
    };

    void timerCallback() override;
    void analyzeWindow();

    SoundXAudioProcessor& processor_;
    std::vector<float> window_;
    int windowFill_ = 0;
    std::array<Body, kMaxBodies> bodies_{};
    float fundamentalHz_ = 0.0f;
    float consonance_ = 1.0f;
    float yaw_ = 0.0f;
    juce::String intervalText_;
    float level_ = 0.0f; // smoothed output level for the core glow

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OrbitView)
};
