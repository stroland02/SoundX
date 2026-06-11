#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cmath>
#include <cstdint>
#include <numbers>
#include "engine/SampleData.h"
#include "engine/SampleImporter.h"

using namespace soundx::engine;
using Catch::Approx;

namespace {
SampleData sineSample(double hz, double sr, std::size_t n) {
    SampleData s;
    s.sourceSampleRate = sr;
    s.samples.resize(n);
    for (std::size_t i = 0; i < n; ++i)
        s.samples[i] = float(std::sin(2.0 * std::numbers::pi * hz * double(i) / sr));
    return s;
}
} // namespace

TEST_CASE("sine sample becomes a bank of sine tables") {
    const auto s = sineSample(440.0, 44100.0, 44100); // 1 second
    const auto wt = makeWavetableFromSample(s);
    REQUIRE(wt.numTables() == SampleImporterDefaults::kNumSlices);
    // each slice of a steady sine should itself be (close to) one sine cycle:
    // peak amplitude ~1 after per-table normalization.
    float peak = 0.0f;
    for (std::size_t i = 0; i < Wavetable::kTableSize; ++i)
        peak = std::max(peak, std::abs(wt.sample(float(i) / float(Wavetable::kTableSize), 0.0f)));
    REQUIRE(peak == Approx(1.0f).margin(0.05f));
}

TEST_CASE("unpitched noise falls back to raw slices without crashing") {
    SampleData s;
    s.sourceSampleRate = 44100.0;
    s.samples.resize(32768);
    std::uint32_t seed = 7;
    for (auto& v : s.samples) {
        seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
        v = float(seed) / float(UINT32_MAX) * 2.0f - 1.0f;
    }
    const auto wt = makeWavetableFromSample(s);
    REQUIRE(wt.numTables() == SampleImporterDefaults::kNumSlices);
    // bounded output
    for (std::size_t i = 0; i < Wavetable::kTableSize; i += 16)
        REQUIRE(std::abs(wt.sample(float(i) / float(Wavetable::kTableSize), 0.5f)) <= 1.0001f);
}

TEST_CASE("tiny or empty samples produce the factory bank instead of garbage") {
    SampleData s;
    s.sourceSampleRate = 44100.0;
    s.samples.resize(16, 0.5f);
    const auto wt = makeWavetableFromSample(s);
    REQUIRE(wt.numTables() >= 2); // falls back to factory sine/saw
}
