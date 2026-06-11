#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cmath>
#include <numbers>
#include <vector>
#include "engine/SampleData.h"
#include "engine/SpectralModel.h"
#include "engine/SpectralVoice.h"

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

// ---------- SpectralVoice ----------

namespace {
// constant single-partial model: 440 Hz, amp 1, 2 seconds worth of frames
SpectralModel constantModel(float hz = 440.0f, float amp = 1.0f) {
    SpectralModel m;
    m.framesPerSecond = 86.13;
    m.frames.resize(172);
    for (auto& f : m.frames)
        f.partials[0] = {hz, amp};
    return m;
}

int upwardCrossings(const std::vector<float>& s) {
    int n = 0;
    for (std::size_t i = 1; i < s.size(); ++i)
        if (s[i - 1] <= 0.0f && s[i] > 0.0f)
            ++n;
    return n;
}

float rmsOf(const std::vector<float>& v) {
    double acc = 0.0;
    for (float x : v)
        acc += double(x) * x;
    return float(std::sqrt(acc / double(v.size())));
}
} // namespace

TEST_CASE("spectral voice without a model is silent and safe") {
    SpectralVoice voice;
    voice.setSampleRate(44100.0);
    voice.noteOn(60, 1.0f);
    std::vector<float> out(1024, 0.0f);
    voice.render(out.data(), int(out.size()));
    REQUIRE(rmsOf(out) == 0.0f);
}

TEST_CASE("spectral voice resynthesizes a single partial at its frequency") {
    const auto m = constantModel();
    SpectralVoice voice;
    voice.setSampleRate(44100.0);
    voice.setModel(&m);
    voice.setEnvParams(0.001f, 0.05f, 1.0f, 0.05f);
    voice.setSpectralParams(1.0f); // stretch
    voice.noteOn(60, 1.0f);       // original pitch

    std::vector<float> out(44100, 0.0f);
    voice.render(out.data(), int(out.size()));
    REQUIRE(rmsOf(out) > 0.3f);
    const int crossings = upwardCrossings(out);
    REQUIRE(crossings >= 435);
    REQUIRE(crossings <= 445);
    for (float v : out)
        REQUIRE(std::isfinite(v));
}

TEST_CASE("spectral voice pitch-tracks the keyboard") {
    const auto m = constantModel();
    auto crossingsFor = [&](int note) {
        SpectralVoice voice;
        voice.setSampleRate(44100.0);
        voice.setModel(&m);
        voice.setEnvParams(0.001f, 0.05f, 1.0f, 0.05f);
        voice.setSpectralParams(1.0f);
        voice.noteOn(note, 1.0f);
        std::vector<float> out(44100, 0.0f);
        voice.render(out.data(), int(out.size()));
        return upwardCrossings(out);
    };
    const int base = crossingsFor(60);
    const int octave = crossingsFor(72);
    REQUIRE(octave > int(double(base) * 1.9));
    REQUIRE(octave < int(double(base) * 2.1));
}

TEST_CASE("spectral voice releases to silence") {
    const auto m = constantModel();
    SpectralVoice voice;
    voice.setSampleRate(44100.0);
    voice.setModel(&m);
    voice.setEnvParams(0.005f, 0.05f, 0.8f, 0.03f);
    voice.setSpectralParams(1.0f);
    voice.noteOn(60, 1.0f);
    std::vector<float> warm(4410, 0.0f);
    voice.render(warm.data(), int(warm.size()));

    voice.noteOff();
    std::vector<float> tail(8820, 0.0f);
    voice.render(tail.data(), int(tail.size()));
    REQUIRE_FALSE(voice.isActive());

    std::vector<float> after(512, 0.0f);
    voice.render(after.data(), int(after.size()));
    REQUIRE(rmsOf(after) == 0.0f);
}

TEST_CASE("spectral model morph glides partial frequencies") {
    const auto mA = constantModel(440.0f);
    const auto mB = constantModel(660.0f);
    auto crossingsAt = [&](float morph) {
        SpectralVoice voice;
        voice.setSampleRate(44100.0);
        voice.setModel(&mA);
        voice.setModelB(&mB);
        voice.setMorph(morph);
        voice.setEnvParams(0.001f, 0.05f, 1.0f, 0.05f);
        voice.setSpectralParams(1.0f);
        voice.noteOn(60, 1.0f);
        std::vector<float> out(44100, 0.0f);
        voice.render(out.data(), int(out.size()));
        for (float v : out)
            REQUIRE(std::isfinite(v));
        return upwardCrossings(out);
    };
    const int at0 = crossingsAt(0.0f);
    const int atHalf = crossingsAt(0.5f);
    const int at1 = crossingsAt(1.0f);
    REQUIRE(at0 >= 435);  REQUIRE(at0 <= 445);   // pure A: 440
    REQUIRE(atHalf >= 543); REQUIRE(atHalf <= 557); // glide midpoint: 550, NOT both tones
    REQUIRE(at1 >= 653);  REQUIRE(at1 <= 667);   // pure B: 660
}

TEST_CASE("stretch 0 freezes the frame but keeps sounding") {
    const auto m = constantModel();
    SpectralVoice voice;
    voice.setSampleRate(44100.0);
    voice.setModel(&m);
    voice.setEnvParams(0.001f, 0.05f, 1.0f, 0.05f);
    voice.setSpectralParams(0.0f); // freeze
    voice.noteOn(60, 1.0f);
    std::vector<float> out(22050, 0.0f); // 500ms — way past one frame
    voice.render(out.data(), int(out.size()));
    REQUIRE(rmsOf(out) > 0.3f);
    REQUIRE(voice.isActive());
}
