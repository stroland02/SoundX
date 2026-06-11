#pragma once
// JUCE-free. Offline conversion of a decoded sample into engine assets.
// Call from background/import threads only — allocates freely.
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>
#include "PitchDetector.h"
#include "SampleData.h"
#include "Wavetable.h"

namespace soundx::engine {

struct SampleImporterDefaults {
    static constexpr std::size_t kNumSlices = 8;   // tables across the sample timeline
    static constexpr std::size_t kAnalysisWindow = 4096;
    static constexpr std::size_t kMinUsableSamples = 256;
};

namespace detail {

// Resample src[start .. start+srcLen) to exactly Wavetable::kTableSize points
// (linear interpolation), normalized to peak 1.
inline Wavetable::Table sliceToTable(const std::vector<float>& src, double start, double srcLen) {
    Wavetable::Table table(Wavetable::kTableSize, 0.0f);
    if (srcLen < 2.0 || src.empty())
        return table;
    const double maxIndex = double(src.size()) - 1.0;
    float peak = 0.0f;
    for (std::size_t i = 0; i < Wavetable::kTableSize; ++i) {
        const double pos = std::min(start + srcLen * double(i) / double(Wavetable::kTableSize), maxIndex);
        const auto i0 = std::size_t(pos);
        const auto i1 = std::min(i0 + 1, src.size() - 1);
        const float frac = float(pos - double(i0));
        table[i] = src[i0] * (1.0f - frac) + src[i1] * frac;
        peak = std::max(peak, std::abs(table[i]));
    }
    if (peak > 1e-6f)
        for (auto& v : table)
            v /= peak;
    return table;
}

} // namespace detail

// Builds a wavetable bank whose position axis scans the sample's evolution:
// 8 single-cycle slices taken at evenly spaced points through the file.
// Unpitched material falls back to raw kTableSize-sample slices; unusable
// input falls back to the factory sine/saw bank.
inline Wavetable makeWavetableFromSample(const SampleData& s) {
    const auto& x = s.samples;
    if (x.size() < SampleImporterDefaults::kMinUsableSamples)
        return Wavetable::makeSineSaw();

    // Detect pitch on a window from the middle of the file (steadier than the onset).
    const auto window = std::min(SampleImporterDefaults::kAnalysisWindow, x.size());
    const auto windowStart = (x.size() - window) / 2;
    const float period = detectPeriod(x.data() + windowStart, window, s.sourceSampleRate);

    const double sliceLen = (period >= 2.0f) ? double(period) : double(Wavetable::kTableSize);
    if (double(x.size()) < sliceLen + 1.0)
        return Wavetable::makeSineSaw();

    Wavetable wt;
    const double lastStart = double(x.size()) - sliceLen - 1.0;
    for (std::size_t k = 0; k < SampleImporterDefaults::kNumSlices; ++k) {
        const double start = (SampleImporterDefaults::kNumSlices == 1)
            ? 0.0
            : lastStart * double(k) / double(SampleImporterDefaults::kNumSlices - 1);
        wt.addTable(detail::sliceToTable(x, start, sliceLen));
    }
    return wt;
}

} // namespace soundx::engine
