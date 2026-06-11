#pragma once
// JUCE-free. Offline STFT analysis into time-varying partial frames.
// analyzeSpectral() allocates freely — background/import threads only.
// The resulting SpectralModel is immutable and safe to read from the audio thread.
#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <numbers>
#include <vector>
#include "Fft.h"
#include "SampleData.h"

namespace soundx::engine {

struct SpectralPartial {
    float freqHz = 0.0f;
    float amp = 0.0f;
};

struct SpectralFrame {
    static constexpr std::size_t kMaxPartials = 32;
    std::array<SpectralPartial, kMaxPartials> partials{}; // sorted by freq ascending
};

struct SpectralModel {
    std::vector<SpectralFrame> frames;
    double framesPerSecond = 0.0;
};

namespace spectral_detail {
constexpr std::size_t kWindowSize = 2048;
constexpr std::size_t kHopSize = 512;
} // namespace spectral_detail

inline SpectralModel analyzeSpectral(const SampleData& s) {
    using namespace spectral_detail;
    SpectralModel model;
    const auto& x = s.samples;
    if (x.size() < kWindowSize || s.sourceSampleRate <= 0.0)
        return model;

    model.framesPerSecond = s.sourceSampleRate / double(kHopSize);

    // Hann window, precomputed
    std::array<double, kWindowSize> window{};
    for (std::size_t i = 0; i < kWindowSize; ++i)
        window[i] = 0.5 * (1.0 - std::cos(2.0 * std::numbers::pi * double(i) / double(kWindowSize)));

    std::vector<std::complex<double>> spectrum(kWindowSize);
    std::vector<double> mags(kWindowSize / 2);

    for (std::size_t start = 0; start + kWindowSize <= x.size(); start += kHopSize) {
        for (std::size_t i = 0; i < kWindowSize; ++i)
            spectrum[i] = {double(x[start + i]) * window[i], 0.0};
        fft(spectrum);

        for (std::size_t b = 0; b < mags.size(); ++b)
            mags[b] = std::abs(spectrum[b]);

        // local maxima -> candidate peaks
        struct Peak { double freq; double amp; };
        std::vector<Peak> peaks;
        for (std::size_t b = 2; b + 2 < mags.size(); ++b) {
            if (mags[b] <= mags[b - 1] || mags[b] < mags[b + 1])
                continue;
            // parabolic interpolation around the peak bin
            const double alpha = mags[b - 1], beta = mags[b], gamma = mags[b + 1];
            const double denom = alpha - 2.0 * beta + gamma;
            const double delta = (denom != 0.0) ? 0.5 * (alpha - gamma) / denom : 0.0;
            const double freq = (double(b) + delta) * s.sourceSampleRate / double(kWindowSize);
            const double mag = beta - 0.25 * (alpha - gamma) * delta;
            // Hann coherent gain 0.5, single-sided: sine of amplitude A peaks at A*N/4
            const double amp = 4.0 * mag / double(kWindowSize);
            peaks.push_back({freq, amp});
        }

        std::sort(peaks.begin(), peaks.end(),
                  [](const Peak& a, const Peak& b) { return a.amp > b.amp; });
        if (peaks.size() > SpectralFrame::kMaxPartials)
            peaks.resize(SpectralFrame::kMaxPartials);
        std::sort(peaks.begin(), peaks.end(),
                  [](const Peak& a, const Peak& b) { return a.freq < b.freq; });

        SpectralFrame frame;
        for (std::size_t i = 0; i < peaks.size(); ++i)
            frame.partials[i] = {float(peaks[i].freq), float(peaks[i].amp)};
        model.frames.push_back(frame);
    }
    return model;
}

} // namespace soundx::engine
