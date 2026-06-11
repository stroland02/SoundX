#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cmath>
#include <numbers>
#include <vector>
#include "engine/effects/Chorus.h"
#include "engine/effects/Distortion.h"
#include "engine/effects/MultibandComp.h"
#include "engine/effects/Reverb.h"
#include "engine/effects/StereoDelay.h"

using namespace soundx::engine;
using Catch::Approx;

namespace {
constexpr double kSr = 44100.0;

std::vector<float> sine(double hz, float amp, std::size_t n) {
    std::vector<float> v(n);
    for (std::size_t i = 0; i < n; ++i)
        v[i] = amp * float(std::sin(2.0 * std::numbers::pi * hz * double(i) / kSr));
    return v;
}

float rms(const std::vector<float>& v, std::size_t from = 0, std::size_t to = SIZE_MAX) {
    to = std::min(to, v.size());
    double acc = 0.0;
    for (std::size_t i = from; i < to; ++i)
        acc += double(v[i]) * v[i];
    return float(std::sqrt(acc / double(to - from)));
}

bool allFinite(const std::vector<float>& v) {
    for (float x : v)
        if (!std::isfinite(x))
            return false;
    return true;
}
} // namespace

TEST_CASE("distortion: mix 0 is bit-exact passthrough; output stays bounded") {
    Distortion fx;
    fx.prepare(kSr, 512);

    auto l = sine(220.0, 0.9f, 4096);
    auto r = l;
    const auto dry = l;

    fx.setParams(10.0f, 0.0f);
    fx.process(l.data(), r.data(), int(l.size()));
    for (std::size_t i = 0; i < l.size(); ++i)
        REQUIRE(l[i] == dry[i]);

    fx.setParams(20.0f, 1.0f);
    fx.process(l.data(), r.data(), int(l.size()));
    REQUIRE(allFinite(l));
    for (float x : l)
        REQUIRE(std::abs(x) <= 1.001f);
}

TEST_CASE("distortion adds harmonic content with drive") {
    auto l = sine(220.0, 0.8f, 4096);
    auto r = l;
    const auto dry = l;

    Distortion fx;
    fx.prepare(kSr, 512);
    fx.setParams(12.0f, 1.0f);
    fx.process(l.data(), r.data(), int(l.size()));

    std::vector<float> diff(l.size());
    for (std::size_t i = 0; i < l.size(); ++i)
        diff[i] = l[i] - dry[i];
    REQUIRE(rms(diff) > 0.05f); // waveshaping changed the signal materially
}

TEST_CASE("multiband comp narrows dynamic range; depth 0 passes through") {
    auto runComp = [](float amp, float depth) {
        auto l = sine(440.0, amp, 22050);
        auto r = l;
        MultibandComp fx;
        fx.prepare(kSr, 512);
        fx.setParams(depth);
        fx.process(l.data(), r.data(), int(l.size()));
        REQUIRE(allFinite(l));
        return rms(l, 11025); // settle time before measuring
    };

    const float quietIn = 0.05f, loudIn = 0.8f;
    const float quietOut = runComp(quietIn, 1.0f);
    const float loudOut = runComp(loudIn, 1.0f);
    const float inRatio = loudIn / quietIn;          // 16
    const float outRatio = loudOut / quietOut;
    REQUIRE(outRatio < inRatio * 0.6f); // dynamic range audibly narrowed

    // depth 0: band split sums back to the input exactly (within float noise)
    auto l = sine(440.0, 0.5f, 4096);
    auto r = l;
    const auto dry = l;
    MultibandComp fx;
    fx.prepare(kSr, 512);
    fx.setParams(0.0f);
    fx.process(l.data(), r.data(), int(l.size()));
    for (std::size_t i = 0; i < l.size(); ++i)
        REQUIRE(l[i] == Approx(dry[i]).margin(1e-5f));
}

TEST_CASE("chorus thickens the signal; mix 0 is passthrough") {
    auto l = sine(440.0, 0.5f, 8192);
    auto r = l;
    const auto dry = l;

    Chorus fx;
    fx.prepare(kSr, 512);
    fx.setParams(1.0f, 8.0f, 0.0f);
    fx.process(l.data(), r.data(), int(l.size()));
    for (std::size_t i = 0; i < l.size(); ++i)
        REQUIRE(l[i] == dry[i]);

    fx.setParams(1.0f, 8.0f, 0.7f);
    fx.process(l.data(), r.data(), int(l.size()));
    REQUIRE(allFinite(l));
    std::vector<float> diff(l.size());
    for (std::size_t i = 0; i < l.size(); ++i)
        diff[i] = l[i] - dry[i];
    REQUIRE(rms(diff, 4096) > 0.01f);
}

TEST_CASE("delay echoes an impulse at the configured time") {
    StereoDelay fx;
    fx.prepare(kSr, 512);
    fx.setParams(100.0f, 0.5f, 1.0f); // 100ms, 50% feedback, full wet

    std::vector<float> l(22050, 0.0f), r(22050, 0.0f);
    l[0] = 1.0f;
    r[0] = 1.0f;
    fx.process(l.data(), r.data(), int(l.size()));
    REQUIRE(allFinite(l));

    const auto echo1 = std::size_t(0.1 * kSr); // 4410
    float peak1 = 0.0f, peak2 = 0.0f;
    for (std::size_t i = echo1 - 50; i < echo1 + 50; ++i)
        peak1 = std::max(peak1, std::abs(l[i]));
    for (std::size_t i = 2 * echo1 - 50; i < 2 * echo1 + 50; ++i)
        peak2 = std::max(peak2, std::abs(l[i]));
    REQUIRE(peak1 > 0.5f);              // first echo strong
    REQUIRE(peak2 > 0.1f);              // feedback echo present
    REQUIRE(peak2 < peak1);             // and decaying
}

TEST_CASE("reverb grows a decaying tail; mix 0 is passthrough") {
    Reverb fx;
    fx.prepare(kSr, 512);

    std::vector<float> l(44100, 0.0f), r(44100, 0.0f);
    l[0] = 1.0f;
    r[0] = 1.0f;
    auto dryL = l;

    fx.setParams(0.6f, 0.4f, 0.0f);
    fx.process(l.data(), r.data(), int(l.size()));
    for (std::size_t i = 0; i < 1000; ++i)
        REQUIRE(l[i] == dryL[i]);

    std::fill(l.begin(), l.end(), 0.0f);
    std::fill(r.begin(), r.end(), 0.0f);
    l[0] = 1.0f;
    r[0] = 1.0f;
    fx.setParams(0.6f, 0.4f, 1.0f);
    fx.process(l.data(), r.data(), int(l.size()));
    REQUIRE(allFinite(l));

    const float early = rms(l, 2205, 8820);    // 50-200ms
    const float late = rms(l, 22050, 30870);   // 500-700ms
    REQUIRE(early > 1e-4f); // a tail exists
    REQUIRE(late < early);  // and it decays
}
