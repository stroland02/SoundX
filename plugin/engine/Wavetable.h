#pragma once
// JUCE-free. Allocation only in addTable()/makeSineSaw() — sample() is RT-safe.
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <vector>

namespace soundx::engine {

class Wavetable {
public:
    static constexpr std::size_t kTableSize = 2048;
    using Table = std::vector<float>;

    void addTable(Table samples) {
        assert(samples.size() == kTableSize);
        tables_.push_back(std::move(samples));
    }

    std::size_t numTables() const { return tables_.size(); }

    // phase in [0,1), position in [0,1] across the bank
    float sample(float phase, float position) const {
        if (tables_.empty())
            return 0.0f;
        const float scaled = std::clamp(position, 0.0f, 1.0f) * float(tables_.size() - 1);
        const auto low = std::min(std::size_t(scaled), tables_.size() - 1);
        const auto high = std::min(low + 1, tables_.size() - 1);
        const float tableFrac = scaled - float(low);
        if (low == high)
            return readTable(tables_[low], phase);
        return readTable(tables_[low], phase) * (1.0f - tableFrac)
             + readTable(tables_[high], phase) * tableFrac;
    }

    // Table 0: pure sine. Table 1: band-limited saw (64 harmonics).
    static Wavetable makeSineSaw() {
        constexpr double twoPi = 2.0 * std::numbers::pi;
        Wavetable wt;
        Table sine(kTableSize), saw(kTableSize, 0.0f);
        constexpr int kHarmonics = 64;
        for (std::size_t i = 0; i < kTableSize; ++i) {
            const double x = double(i) / double(kTableSize);
            sine[i] = float(std::sin(twoPi * x));
            // Band-limited sawtooth with Lanczos sigma windowing to suppress
            // Gibbs overshoot below 1.05. Standard rising saw: -(2/pi)*sum sigma_k*sin(kx)/k
            double s = 0.0;
            for (int k = 1; k <= kHarmonics; ++k) {
                const double sigma = std::sin(std::numbers::pi * k / kHarmonics)
                                   / (std::numbers::pi * k / kHarmonics);
                s += sigma * std::sin(twoPi * k * x) / double(k);
            }
            saw[i] = float(s * (-2.0 / std::numbers::pi));
        }
        wt.addTable(std::move(sine));
        wt.addTable(std::move(saw));
        return wt;
    }

private:
    static float readTable(const Table& t, float phase) {
        assert(std::isfinite(phase));
        const float idx = (phase - std::floor(phase)) * float(kTableSize);
        const auto truncated = std::size_t(idx);
        const auto i0 = truncated % kTableSize;
        const auto i1 = (i0 + 1) % kTableSize;
        const float frac = idx - float(truncated);
        return t[i0] * (1.0f - frac) + t[i1] * frac;
    }

    std::vector<Table> tables_;
};

} // namespace soundx::engine
