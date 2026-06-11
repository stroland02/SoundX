#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cmath>
#include "engine/Lfo.h"

using soundx::engine::Lfo;
using Catch::Approx;

TEST_CASE("sine LFO hits its quarter-phase landmarks") {
    Lfo lfo;
    lfo.setSampleRate(1000.0);
    lfo.setRate(1.0f); // 1 Hz: one period = 1000 samples
    lfo.setShape(Lfo::Shape::sine);

    REQUIRE(lfo.advance(250) == Approx(1.0f).margin(1e-3f));  // quarter
    REQUIRE(lfo.advance(250) == Approx(0.0f).margin(1e-3f));  // half
    REQUIRE(lfo.advance(250) == Approx(-1.0f).margin(1e-3f)); // three-quarter
    REQUIRE(lfo.advance(250) == Approx(0.0f).margin(1e-3f));  // full
}

TEST_CASE("triangle LFO ramps symmetrically") {
    Lfo lfo;
    lfo.setSampleRate(1000.0);
    lfo.setRate(1.0f);
    lfo.setShape(Lfo::Shape::triangle);

    // triangle starting at -1: phase 0.25 -> 0, 0.5 -> +1, 0.75 -> 0
    REQUIRE(lfo.advance(250) == Approx(0.0f).margin(1e-2f));
    REQUIRE(lfo.advance(250) == Approx(1.0f).margin(1e-2f));
    REQUIRE(lfo.advance(250) == Approx(0.0f).margin(1e-2f));
}

TEST_CASE("saw LFO rises from -1 to 1 over a period") {
    Lfo lfo;
    lfo.setSampleRate(1000.0);
    lfo.setRate(1.0f);
    lfo.setShape(Lfo::Shape::saw);

    REQUIRE(lfo.advance(500) == Approx(0.0f).margin(1e-2f));   // midpoint
    REQUIRE(lfo.advance(499) == Approx(0.998f).margin(5e-3f)); // just before wrap
}

TEST_CASE("square LFO flips sign at half period") {
    Lfo lfo;
    lfo.setSampleRate(1000.0);
    lfo.setRate(1.0f);
    lfo.setShape(Lfo::Shape::square);

    REQUIRE(lfo.advance(100) == 1.0f);  // first half: high
    REQUIRE(lfo.advance(450) == -1.0f); // second half: low
}

TEST_CASE("sample-and-hold stays bounded and changes across wraps") {
    Lfo lfo;
    lfo.setSampleRate(1000.0);
    lfo.setRate(10.0f); // wrap every 100 samples
    lfo.setShape(Lfo::Shape::sampleHold);

    float first = lfo.advance(50);
    bool changed = false;
    for (int i = 0; i < 20; ++i) {
        const float v = lfo.advance(100); // crosses a wrap each time
        REQUIRE(v >= -1.0f);
        REQUIRE(v <= 1.0f);
        if (std::abs(v - first) > 1e-6f)
            changed = true;
    }
    REQUIRE(changed);
}

TEST_CASE("LFO without a sample rate is silent and safe") {
    Lfo lfo;
    lfo.setRate(5.0f);
    REQUIRE(lfo.advance(512) == 0.0f);
}
