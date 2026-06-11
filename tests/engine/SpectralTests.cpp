#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cmath>
#include <numbers>
#include <vector>
#include "engine/SampleData.h"
#include "engine/SpectralModel.h"

using namespace soundx::engine;
using Catch::Approx;

namespace {
SampleData toneSample(std::vector<std::pair<double, double>> freqAmps,
                      double sr = 44100.0, std::size_t n = 44100) {
    SampleData s;
    s.sourceSampleRate = sr;
    s.samples.resize(n, 0.0f);
    for (std::size_t i = 0; i < n; ++i) {
        double v = 0.0;
        for (auto [hz, amp] : freqAmps)
            v += amp * std::sin(2.0 * std::numbers::pi * hz * double(i) / sr);
        s.samples[i] = float(v);
    }
    return s;
}

// strongest partial of a frame
SpectralPartial dominant(const SpectralFrame& f) {
    SpectralPartial best{0.0f, 0.0f};
    for (const auto& p : f.partials)
        if (p.amp > best.amp)
            best = p;
    return best;
}
} // namespace

TEST_CASE("analyzer finds a pure sine as one dominant partial") {
    const auto s = toneSample({{440.0, 1.0}});
    const auto model = analyzeSpectral(s);
    REQUIRE(model.frames.size() > 20);
    REQUIRE(model.framesPerSecond > 40.0);

    const auto& mid = model.frames[model.frames.size() / 2];
    const auto top = dominant(mid);
    REQUIRE(top.freqHz == Approx(440.0f).margin(2.0f));
    REQUIRE(top.amp == Approx(1.0f).margin(0.25f));
}

TEST_CASE("analyzer resolves two simultaneous tones") {
    const auto s = toneSample({{440.0, 0.8}, {1320.0, 0.4}});
    const auto model = analyzeSpectral(s);
    const auto& mid = model.frames[model.frames.size() / 2];

    bool found440 = false, found1320 = false;
    for (const auto& p : mid.partials) {
        if (std::abs(p.freqHz - 440.0f) < 3.0f && p.amp > 0.4f)
            found440 = true;
        if (std::abs(p.freqHz - 1320.0f) < 5.0f && p.amp > 0.2f)
            found1320 = true;
    }
    REQUIRE(found440);
    REQUIRE(found1320);
}

TEST_CASE("analyzer of silence produces near-zero partial amplitudes") {
    SampleData s;
    s.sourceSampleRate = 44100.0;
    s.samples.resize(22050, 0.0f);
    const auto model = analyzeSpectral(s);
    REQUIRE(!model.frames.empty());
    for (const auto& p : model.frames.front().partials)
        REQUIRE(p.amp < 1e-4f);
}

TEST_CASE("analyzer handles too-short input gracefully") {
    SampleData s;
    s.sourceSampleRate = 44100.0;
    s.samples.resize(100, 0.5f);
    const auto model = analyzeSpectral(s);
    REQUIRE(model.frames.empty());
}
