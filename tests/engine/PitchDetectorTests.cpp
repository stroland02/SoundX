#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <vector>
#include "engine/PitchDetector.h"

using namespace soundx::engine;
using Catch::Approx;

namespace {
std::vector<float> sine(double hz, double sr, std::size_t n) {
    std::vector<float> v(n);
    for (std::size_t i = 0; i < n; ++i)
        v[i] = float(std::sin(2.0 * std::numbers::pi * hz * double(i) / sr));
    return v;
}
} // namespace

TEST_CASE("detects the period of a pure sine") {
    const auto x = sine(440.0, 44100.0, 4096);
    const float period = detectPeriod(x.data(), x.size(), 44100.0);
    REQUIRE(period == Approx(44100.0 / 440.0).margin(1.5));
}

TEST_CASE("detects a low note") {
    const auto x = sine(82.4, 44100.0, 8192); // low E
    const float period = detectPeriod(x.data(), x.size(), 44100.0);
    REQUIRE(period == Approx(44100.0 / 82.4).margin(3.0));
}

TEST_CASE("detects a harmonically rich tone at the fundamental") {
    std::vector<float> x(4096);
    for (std::size_t i = 0; i < x.size(); ++i) {
        const double t = double(i) / 44100.0;
        x[i] = float(0.6 * std::sin(2.0 * std::numbers::pi * 220.0 * t)
                   + 0.3 * std::sin(2.0 * std::numbers::pi * 440.0 * t)
                   + 0.1 * std::sin(2.0 * std::numbers::pi * 660.0 * t));
    }
    const float period = detectPeriod(x.data(), x.size(), 44100.0);
    REQUIRE(period == Approx(44100.0 / 220.0).margin(2.0));
}

TEST_CASE("returns 0 for silence and noise") {
    std::vector<float> silence(4096, 0.0f);
    REQUIRE(detectPeriod(silence.data(), silence.size(), 44100.0) == 0.0f);

    std::vector<float> noise(4096);
    std::uint32_t seed = 1;
    for (auto& v : noise) {
        seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
        v = float(seed) / float(UINT32_MAX) * 2.0f - 1.0f;
    }
    REQUIRE(detectPeriod(noise.data(), noise.size(), 44100.0) == 0.0f);
}
