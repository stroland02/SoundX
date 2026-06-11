#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <numbers>
#include <vector>
#include "engine/GranularVoice.h"
#include "engine/SampleData.h"

using namespace soundx::engine;

namespace {
SampleData sineSample() {
    SampleData s;
    s.sourceSampleRate = 44100.0;
    s.samples.resize(44100);
    for (std::size_t i = 0; i < s.samples.size(); ++i)
        s.samples[i] = float(std::sin(2.0 * std::numbers::pi * 440.0 * double(i) / 44100.0));
    return s;
}

float rms(const std::vector<float>& v) {
    double acc = 0.0;
    for (float x : v)
        acc += double(x) * x;
    return float(std::sqrt(acc / double(v.size())));
}
} // namespace

TEST_CASE("granular voice without a sample is silent and safe") {
    GranularVoice voice;
    voice.setSampleRate(44100.0);
    voice.noteOn(60, 1.0f);
    std::vector<float> out(1024, 0.0f);
    voice.render(out.data(), int(out.size()));
    REQUIRE(rms(out) == 0.0f);
}

TEST_CASE("granular voice plays grains from a sample") {
    const auto s = sineSample();
    GranularVoice voice;
    voice.setSampleRate(44100.0);
    voice.setSample(&s);
    voice.setEnvParams(0.005f, 0.05f, 0.8f, 0.05f);
    voice.setGrainParams(100.0f, 40.0f, 0.0f, 0.25f); // sizeMs, densityHz, spray, position
    voice.noteOn(60, 1.0f); // note 60 = original pitch
    REQUIRE(voice.isActive());

    std::vector<float> out(8820, 0.0f); // 200ms
    voice.render(out.data(), int(out.size()));
    REQUIRE(rms(out) > 0.05f);

    for (float v : out)
        REQUIRE(std::isfinite(v));
}

TEST_CASE("granular voice decays to silence after noteOff") {
    const auto s = sineSample();
    GranularVoice voice;
    voice.setSampleRate(44100.0);
    voice.setSample(&s);
    voice.setEnvParams(0.005f, 0.05f, 0.8f, 0.03f);
    voice.setGrainParams(50.0f, 40.0f, 0.2f, 0.5f);
    voice.noteOn(60, 1.0f);
    std::vector<float> warm(4410, 0.0f);
    voice.render(warm.data(), int(warm.size()));

    voice.noteOff();
    std::vector<float> tail(44100, 0.0f); // 1s >> release + max grain length
    voice.render(tail.data(), int(tail.size()));
    REQUIRE_FALSE(voice.isActive());

    std::vector<float> after(512, 0.0f);
    voice.render(after.data(), int(after.size()));
    REQUIRE(rms(after) == 0.0f);
}

TEST_CASE("kill stops the granular voice immediately") {
    const auto s = sineSample();
    GranularVoice voice;
    voice.setSampleRate(44100.0);
    voice.setSample(&s);
    voice.noteOn(60, 1.0f);
    voice.kill();
    REQUIRE_FALSE(voice.isActive());
}

TEST_CASE("higher notes read the sample faster (pitch tracking)") {
    const auto s = sineSample();
    auto countCrossings = [&](int note) {
        GranularVoice voice;
        voice.setSampleRate(44100.0);
        voice.setSample(&s);
        voice.setEnvParams(0.001f, 0.05f, 1.0f, 0.05f);
        voice.setGrainParams(400.0f, 10.0f, 0.0f, 0.0f); // long, sparse, deterministic
        voice.noteOn(note, 1.0f);
        std::vector<float> out(44100, 0.0f);
        voice.render(out.data(), int(out.size()));
        int n = 0;
        for (std::size_t i = 1; i < out.size(); ++i)
            if (out[i - 1] <= 0.0f && out[i] > 0.0f)
                ++n;
        return n;
    };
    const int atPitch = countCrossings(60);
    const int octaveUp = countCrossings(72);
    REQUIRE(octaveUp > int(double(atPitch) * 1.6));
    REQUIRE(octaveUp < int(double(atPitch) * 2.4));
}
