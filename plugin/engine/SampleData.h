#pragma once
// JUCE-free. A decoded mono sample. Produced off the audio thread.
#include <vector>

namespace soundx::engine {

struct SampleData {
    std::vector<float> samples;        // mono, normalized to <= 1.0 peak
    double sourceSampleRate = 44100.0;
};

} // namespace soundx::engine
