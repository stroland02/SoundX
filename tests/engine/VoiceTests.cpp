#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cmath>
#include <vector>
#include "engine/Wavetable.h"
#include "engine/WavetableVoice.h"

using namespace soundx::engine;
using Catch::Approx;

namespace {
float rms(const std::vector<float>& s) {
    double acc = 0.0;
    for (float v : s)
        acc += double(v) * double(v);
    return float(std::sqrt(acc / double(s.size())));
}
} // namespace

TEST_CASE("midi note to frequency") {
    REQUIRE(WavetableVoice::midiNoteToHz(69) == Approx(440.0f));
    REQUIRE(WavetableVoice::midiNoteToHz(81) == Approx(880.0f));
    REQUIRE(WavetableVoice::midiNoteToHz(60) == Approx(261.626f).margin(0.01f));
}

TEST_CASE("voice is silent before noteOn") {
    const auto wt = Wavetable::makeSineSaw();
    WavetableVoice voice(wt);
    voice.setSampleRate(44100.0);
    std::vector<float> out(512, 0.0f);
    voice.render(out.data(), int(out.size()));
    REQUIRE(rms(out) == 0.0f);
    REQUIRE_FALSE(voice.isActive());
}

TEST_CASE("voice produces audio after noteOn and decays after noteOff") {
    const auto wt = Wavetable::makeSineSaw();
    WavetableVoice voice(wt);
    voice.setSampleRate(44100.0);
    voice.setParams(0.005f, 0.05f, 0.7f, 0.05f, 0.0f);
    voice.noteOn(69, 1.0f);
    REQUIRE(voice.isActive());

    std::vector<float> out(4410, 0.0f); // 100ms
    voice.render(out.data(), int(out.size()));
    REQUIRE(rms(out) > 0.1f);

    voice.noteOff();
    // render 200ms: release is 50ms so the tail must fully die
    std::vector<float> tail(8820, 0.0f);
    voice.render(tail.data(), int(tail.size()));
    REQUIRE_FALSE(voice.isActive());

    std::vector<float> after(512, 0.0f);
    voice.render(after.data(), int(after.size()));
    REQUIRE(rms(after) == 0.0f);
}

TEST_CASE("render is additive into the destination buffer") {
    const auto wt = Wavetable::makeSineSaw();
    WavetableVoice voice(wt);
    voice.setSampleRate(44100.0);
    voice.setParams(0.0f, 0.05f, 1.0f, 0.05f, 0.0f);
    voice.noteOn(69, 1.0f);

    std::vector<float> out(64, 5.0f); // pre-filled
    voice.render(out.data(), int(out.size()));
    // every sample should still carry the 5.0 offset (synth adds, doesn't overwrite)
    for (float v : out)
        REQUIRE(v >= 3.5f);
}

TEST_CASE("kill silences the voice immediately") {
    const auto wt = Wavetable::makeSineSaw();
    WavetableVoice voice(wt);
    voice.setSampleRate(44100.0);
    voice.noteOn(69, 1.0f);
    voice.kill();
    REQUIRE_FALSE(voice.isActive());
}
