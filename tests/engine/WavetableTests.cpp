#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cmath>
#include "engine/Wavetable.h"

using soundx::engine::Wavetable;
using Catch::Approx;

namespace {
Wavetable::Table rampTable() {
    // table[i] = i / kTableSize, so interpolation is easy to predict
    Wavetable::Table t(Wavetable::kTableSize);
    for (std::size_t i = 0; i < t.size(); ++i)
        t[i] = float(i) / float(Wavetable::kTableSize);
    return t;
}
} // namespace

TEST_CASE("empty wavetable returns silence") {
    Wavetable wt;
    REQUIRE(wt.numTables() == 0);
    REQUIRE(wt.sample(0.5f, 0.0f) == 0.0f);
}

TEST_CASE("reads single table with linear interpolation within the table") {
    Wavetable wt;
    wt.addTable(rampTable());
    // phase 0.5 -> index 1024 exactly -> 1024/2048 = 0.5
    REQUIRE(wt.sample(0.5f, 0.0f) == Approx(0.5f).margin(1e-4f));
    // halfway between index 0 (0.0) and index 1 (1/2048)
    const float halfIndexPhase = 0.5f / float(Wavetable::kTableSize);
    REQUIRE(wt.sample(halfIndexPhase, 0.0f) == Approx(0.5f / float(Wavetable::kTableSize)).margin(1e-5f));
}

TEST_CASE("interpolates between adjacent tables by position") {
    Wavetable wt;
    Wavetable::Table zeros(Wavetable::kTableSize, 0.0f);
    Wavetable::Table ones(Wavetable::kTableSize, 1.0f);
    wt.addTable(zeros);
    wt.addTable(ones);
    REQUIRE(wt.sample(0.3f, 0.0f) == Approx(0.0f).margin(1e-6f));
    REQUIRE(wt.sample(0.3f, 1.0f) == Approx(1.0f).margin(1e-6f));
    REQUIRE(wt.sample(0.3f, 0.5f) == Approx(0.5f).margin(1e-6f));
}

TEST_CASE("factory bank has sine then saw, both bounded") {
    const auto wt = Wavetable::makeSineSaw();
    REQUIRE(wt.numTables() == 2);
    // position 0 is a sine: peak of 1 at phase 0.25
    REQUIRE(wt.sample(0.25f, 0.0f) == Approx(1.0f).margin(1e-3f));
    REQUIRE(wt.sample(0.0f, 0.0f) == Approx(0.0f).margin(1e-3f));
    // every sample of every table stays in [-1.05, 1.05] (Gibbs ripple allowance)
    for (float pos : {0.0f, 1.0f})
        for (std::size_t i = 0; i < Wavetable::kTableSize; ++i) {
            const float v = wt.sample(float(i) / float(Wavetable::kTableSize), pos);
            REQUIRE(std::abs(v) <= 1.05f);
        }
}
