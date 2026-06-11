#pragma once
// JUCE-free iterative radix-2 FFT. Offline analysis only — allocates, not RT-safe.
#include <cassert>
#include <cmath>
#include <complex>
#include <cstddef>
#include <numbers>
#include <vector>

namespace soundx::engine {

// In-place complex FFT; size must be a power of two.
inline void fft(std::vector<std::complex<double>>& a) {
    const std::size_t n = a.size();
    assert(n != 0 && (n & (n - 1)) == 0);

    // bit-reversal permutation
    for (std::size_t i = 1, j = 0; i < n; ++i) {
        std::size_t bit = n >> 1;
        for (; j & bit; bit >>= 1)
            j ^= bit;
        j ^= bit;
        if (i < j)
            std::swap(a[i], a[j]);
    }

    for (std::size_t len = 2; len <= n; len <<= 1) {
        const double angle = -2.0 * std::numbers::pi / double(len);
        const std::complex<double> wlen(std::cos(angle), std::sin(angle));
        for (std::size_t i = 0; i < n; i += len) {
            std::complex<double> w(1.0, 0.0);
            for (std::size_t k = 0; k < len / 2; ++k) {
                const auto u = a[i + k];
                const auto v = a[i + k + len / 2] * w;
                a[i + k] = u + v;
                a[i + k + len / 2] = u - v;
                w *= wlen;
            }
        }
    }
}

} // namespace soundx::engine
