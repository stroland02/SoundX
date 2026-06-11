#pragma once
// JUCE-free. Offline normalized-autocorrelation period estimator.
// Not RT-safe by necessity (O(n * lags)) — call from import/background paths only.
#include <algorithm>
#include <cmath>
#include <cstddef>

namespace soundx::engine {

// Returns the fundamental period in samples, or 0 if no reliable pitch found.
inline float detectPeriod(const float* x, std::size_t n, double sampleRate,
                          float minHz = 50.0f, float maxHz = 2000.0f) {
    if (x == nullptr || n < 64 || sampleRate <= 0.0)
        return 0.0f;

    const auto minLag = std::max<std::size_t>(2, std::size_t(sampleRate / maxHz));
    const auto maxLag = std::min(n / 2, std::size_t(sampleRate / minHz));
    if (minLag >= maxLag)
        return 0.0f;

    double energy = 0.0;
    for (std::size_t i = 0; i < n; ++i)
        energy += double(x[i]) * x[i];
    if (energy < 1e-9)
        return 0.0f; // silence

    double bestR = 0.0;
    std::size_t bestLag = 0;
    for (auto lag = minLag; lag <= maxLag; ++lag) {
        double acc = 0.0, e1 = 0.0, e2 = 0.0;
        for (std::size_t i = 0; i + lag < n; ++i) {
            acc += double(x[i]) * x[i + lag];
            e1 += double(x[i]) * x[i];
            e2 += double(x[i + lag]) * x[i + lag];
        }
        const double norm = std::sqrt(e1 * e2);
        const double r = norm > 0.0 ? acc / norm : 0.0;
        if (r > bestR) {
            bestR = r;
            bestLag = lag;
        }
    }

    // Below this correlation the signal is effectively unpitched.
    constexpr double kVoicedThreshold = 0.8;
    if (bestR < kVoicedThreshold || bestLag == 0)
        return 0.0f;

    // Octave-error guard: if an integer fraction of the best lag correlates
    // almost as well, the true period is that smaller lag. Only genuine
    // subdivision candidates (bestLag/4, /3, /2 +-2 samples) are considered so
    // neighboring lags can't undercut the true peak.
    auto correlationAt = [&](std::size_t lag) {
        double acc = 0.0, e1 = 0.0, e2 = 0.0;
        for (std::size_t i = 0; i + lag < n; ++i) {
            acc += double(x[i]) * x[i + lag];
            e1 += double(x[i]) * x[i];
            e2 += double(x[i + lag]) * x[i + lag];
        }
        const double norm = std::sqrt(e1 * e2);
        return norm > 0.0 ? acc / norm : 0.0;
    };
    for (std::size_t divisor = 4; divisor >= 2; --divisor) {
        const auto center = bestLag / divisor;
        if (center < minLag)
            continue;
        double candR = 0.0;
        std::size_t candLag = 0;
        const auto lo = std::max(minLag, center >= 2 ? center - 2 : minLag);
        for (auto lag = lo; lag <= center + 2 && lag < bestLag; ++lag) {
            const double r = correlationAt(lag);
            if (r > candR) {
                candR = r;
                candLag = lag;
            }
        }
        if (candLag != 0 && candR > bestR * 0.97) {
            bestLag = candLag;
            break;
        }
    }
    return float(bestLag);
}

} // namespace soundx::engine
