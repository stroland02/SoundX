#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cmath>
#include <vector>
#include "engine/Wavetable.h"
#include "engine/WavetableOscillator.h"

using namespace soundx::engine;
using Catch::Approx;

namespace {
int countUpwardZeroCrossings(const std::vector<float>& s) {
    int n = 0;
    for (std::size_t i = 1; i < s.size(); ++i)
        if (s[i - 1] <= 0.0f && s[i] > 0.0f)
            ++n;
    return n;
}
} // namespace

TEST_CASE("oscillator produces requested frequency") {
    const auto wt = Wavetable::makeSineSaw();
    WavetableOscillator osc(wt);
    osc.setSampleRate(44100.0);
    osc.setFrequency(440.0f);
    osc.setPosition(0.0f); // pure sine

    std::vector<float> out(44100);
    for (auto& v : out)
        v = osc.nextSample();

    const int crossings = countUpwardZeroCrossings(out);
    REQUIRE(crossings >= 438);
    REQUIRE(crossings <= 442);
}

TEST_CASE("position 0 follows the sine table") {
    const auto wt = Wavetable::makeSineSaw();
    WavetableOscillator osc(wt);
    osc.setSampleRate(44100.0);
    // one full cycle in exactly 4 samples -> phases 0, .25, .5, .75
    osc.setFrequency(44100.0f / 4.0f);
    osc.setPosition(0.0f);

    REQUIRE(osc.nextSample() == Approx(0.0f).margin(1e-3f));  // sin(0)
    REQUIRE(osc.nextSample() == Approx(1.0f).margin(1e-3f));  // sin(pi/2)
    REQUIRE(osc.nextSample() == Approx(0.0f).margin(1e-3f));  // sin(pi)
    REQUIRE(osc.nextSample() == Approx(-1.0f).margin(1e-3f)); // sin(3pi/2)
}

TEST_CASE("position changes the waveform") {
    const auto wt = Wavetable::makeSineSaw();
    WavetableOscillator sineOsc(wt), sawOsc(wt);
    for (auto* o : {&sineOsc, &sawOsc}) {
        o->setSampleRate(44100.0);
        o->setFrequency(100.0f);
    }
    sineOsc.setPosition(0.0f);
    sawOsc.setPosition(1.0f);

    float maxDiff = 0.0f;
    for (int i = 0; i < 441; ++i) // one cycle
        maxDiff = std::max(maxDiff, std::abs(sineOsc.nextSample() - sawOsc.nextSample()));
    REQUIRE(maxDiff > 0.1f);
}

TEST_CASE("reset returns phase to zero") {
    const auto wt = Wavetable::makeSineSaw();
    WavetableOscillator osc(wt);
    osc.setSampleRate(44100.0);
    osc.setFrequency(440.0f);
    osc.setPosition(0.0f);
    const float first = osc.nextSample();
    for (int i = 0; i < 100; ++i)
        osc.nextSample();
    osc.reset();
    REQUIRE(osc.nextSample() == Approx(first).margin(1e-6f));
}

TEST_CASE("oscillator without a sample rate stays silent and finite") {
    const auto wt = Wavetable::makeSineSaw();
    WavetableOscillator osc(wt);
    osc.setFrequency(440.0f); // no setSampleRate yet
    for (int i = 0; i < 64; ++i) {
        const float v = osc.nextSample();
        REQUIRE(std::isfinite(v));
    }
}
