#pragma once
// JUCE-free psychoacoustic consonance analysis over a set of partials.
// Plomp-Levelt pairwise roughness; suitable for control-rate / UI use.
#include <algorithm>
#include <cmath>
#include <cstddef>
#include "../SpectralModel.h"

namespace soundx::engine {

// Plomp-Levelt roughness contribution of one partial pair (0 = smooth).
inline float pairRoughness(const SpectralPartial& a, const SpectralPartial& b) {
    const float fmin = std::min(a.freqHz, b.freqHz);
    const float diff = std::abs(a.freqHz - b.freqHz);
    if (fmin <= 0.0f)
        return 0.0f;
    const float s = 0.24f / (0.021f * fmin + 19.0f);
    const float x = s * diff;
    const float r = std::exp(-3.5f * x) - std::exp(-5.75f * x);
    return std::max(0.0f, r) * a.amp * b.amp;
}

// Total roughness of a partial set (pairwise sum).
inline float totalRoughness(const SpectralPartial* partials, std::size_t n) {
    float acc = 0.0f;
    for (std::size_t i = 0; i < n; ++i)
        for (std::size_t j = i + 1; j < n; ++j)
            acc += pairRoughness(partials[i], partials[j]);
    return acc;
}

// Consonance in (0, 1]: 1 = perfectly smooth, falls toward 0 with roughness.
inline float consonanceScore01(const SpectralPartial* partials, std::size_t n) {
    const float rough = totalRoughness(partials, n);
    return 1.0f / (1.0f + 8.0f * rough);
}

struct IntervalInfo {
    const char* name;
    float cents; // deviation from the tempered interval
};

// Nearest 12-TET interval for a frequency ratio (octave-reduced names except
// the octave itself).
inline IntervalInfo nearestInterval(float ratio) {
    static constexpr const char* kNames[13] = {"Unison", "m2", "M2", "m3", "M3", "P4", "TT",
                                               "P5", "m6", "M6", "m7", "M7", "Octave"};
    if (ratio <= 0.0f)
        return {"Unison", 0.0f};
    float cents = 1200.0f * std::log2(ratio);
    // reduce to one octave, keeping 1200 itself as Octave
    while (cents > 1250.0f)
        cents -= 1200.0f;
    while (cents < -50.0f)
        cents += 1200.0f;
    const int idx = std::clamp(int(std::round(cents / 100.0f)), 0, 12);
    return {kNames[idx], cents - float(idx) * 100.0f};
}

} // namespace soundx::engine
