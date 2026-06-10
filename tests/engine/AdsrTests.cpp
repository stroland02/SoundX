#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "engine/Adsr.h"

using soundx::engine::Adsr;
using Catch::Approx;

namespace {
Adsr makeEnv() {
    Adsr env;
    env.setSampleRate(1000.0); // 1ms per sample: easy math
    // attack 100ms, decay 100ms, sustain 0.5, release 200ms
    env.setParams(0.1f, 0.1f, 0.5f, 0.2f);
    return env;
}
} // namespace

TEST_CASE("idle envelope outputs zero and is inactive") {
    auto env = makeEnv();
    REQUIRE_FALSE(env.isActive());
    REQUIRE(env.nextLevel() == 0.0f);
}

TEST_CASE("attack reaches 1.0 in the configured time") {
    auto env = makeEnv();
    env.noteOn();
    REQUIRE(env.isActive());
    float level = 0.0f;
    for (int i = 0; i < 100; ++i) // 100 samples @ 1kHz = 100ms
        level = env.nextLevel();
    REQUIRE(level == Approx(1.0f).margin(0.02f));
}

TEST_CASE("decay settles at sustain level") {
    auto env = makeEnv();
    env.noteOn();
    float level = 0.0f;
    for (int i = 0; i < 250; ++i) // attack (100) + decay (100) + margin
        level = env.nextLevel();
    REQUIRE(level == Approx(0.5f).margin(0.02f));
    // sustain holds indefinitely
    for (int i = 0; i < 500; ++i)
        level = env.nextLevel();
    REQUIRE(level == Approx(0.5f).margin(0.02f));
}

TEST_CASE("release decays to zero and deactivates") {
    auto env = makeEnv();
    env.noteOn();
    for (int i = 0; i < 250; ++i)
        env.nextLevel(); // reach sustain
    env.noteOff();
    float level = 1.0f;
    for (int i = 0; i < 220; ++i) // release 200ms + margin
        level = env.nextLevel();
    REQUIRE(level == 0.0f);
    REQUIRE_FALSE(env.isActive());
}

TEST_CASE("reset kills the envelope immediately") {
    auto env = makeEnv();
    env.noteOn();
    for (int i = 0; i < 50; ++i)
        env.nextLevel();
    env.reset();
    REQUIRE_FALSE(env.isActive());
    REQUIRE(env.nextLevel() == 0.0f);
}
