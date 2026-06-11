# SoundX Plan 2a: Granular Engine + Universal Sample Import

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Drag any audio file into SoundX and play it through either engine: as a wavetable bank (8 slices scanning the sample's timbral evolution) or as a granular cloud — the first half of the spec's "one sample becomes all three engines."

**Architecture:** The JUCE-free engine gains a `SoundSource` interface, an offline `SampleImporter` (pitch detection → single-cycle wavetable bank; raw buffer → grain source), and a `GranularVoice` with a pre-allocated grain pool. The plugin decodes files on a background thread (JUCE side), then atomically swaps engine data under `suspendProcessing`. A `mode` parameter switches Wavetable/Granular per-voice.

**Tech Stack:** Same as Plan 1 (C++20, JUCE 8, Catch2, CMake). No new dependencies.

**Spec:** `docs/superpowers/specs/2026-06-10-soundx-synth-design.md` · **Prereq:** Plan 1 merged (23 tests green).

**Deferred to Plan 2b:** spectral engine (partial-tracking analysis + additive resynthesis), per-slot A/B engines, morph field.

---

## File structure

```
plugin/engine/
├── SoundSource.h        # NEW: common voice interface
├── SampleData.h         # NEW: decoded mono sample + rate
├── PitchDetector.h      # NEW: normalized autocorrelation period estimate
├── SampleImporter.h     # NEW: SampleData -> Wavetable bank (8 slices)
├── GranularVoice.h      # NEW: grain pool engine voice
├── Rng.h                # NEW: xorshift32 (RT-safe randomness)
├── WavetableOscillator.h# MOD: setTable() retarget
└── WavetableVoice.h     # MOD: implements SoundSource, setWavetable()
plugin/
├── PluginProcessor.h/.cpp # MOD: mode+granular params, sample load/swap
├── SynthVoice.h           # MOD: owns both engine voices, mode dispatch
└── PluginEditor.h/.cpp    # MOD: drag-drop, mode combo, 3 new knobs
tests/engine/
├── PitchDetectorTests.cpp # NEW
├── ImporterTests.cpp      # NEW
└── GranularTests.cpp      # NEW
tests/plugin/RenderTests.cpp # MOD: granular mode render test
```

---

### Task 1: `SoundSource` interface + retargetable wavetable voice

**Files:**
- Create: `plugin/engine/SoundSource.h`
- Create: `plugin/engine/SampleData.h`
- Modify: `plugin/engine/WavetableOscillator.h` (add `setTable`)
- Modify: `plugin/engine/WavetableVoice.h` (implement SoundSource, add `setWavetable`)

- [ ] **Step 1: Branch**

```powershell
git checkout -b feat/plan2a-granular-import
```

- [ ] **Step 2: Write `plugin/engine/SoundSource.h`**

```cpp
#pragma once
// JUCE-free. Common interface for all engine voices (wavetable/granular/spectral).

namespace soundx::engine {

class SoundSource {
public:
    virtual ~SoundSource() = default;
    virtual void setSampleRate(double sampleRate) = 0;
    virtual void noteOn(int midiNote, float velocity01) = 0;
    virtual void noteOff() = 0;
    virtual void kill() = 0;
    virtual bool isActive() const = 0;
    // Adds into dest; never allocates; numSamples bounded by caller.
    virtual void render(float* dest, int numSamples) = 0;
};

} // namespace soundx::engine
```

- [ ] **Step 3: Write `plugin/engine/SampleData.h`**

```cpp
#pragma once
// JUCE-free. A decoded mono sample. Produced off the audio thread.
#include <vector>

namespace soundx::engine {

struct SampleData {
    std::vector<float> samples;        // mono, normalized to <= 1.0 peak
    double sourceSampleRate = 44100.0;
};

} // namespace soundx::engine
```

- [ ] **Step 4: In `plugin/engine/WavetableOscillator.h`** add after `reset()`:

```cpp
    // Retarget to a different (already-built) bank. Safe between blocks only.
    void setTable(const Wavetable* wavetable) {
        if (wavetable != nullptr)
            table_ = wavetable;
    }
```

- [ ] **Step 5: In `plugin/engine/WavetableVoice.h`**: make the class implement the interface and add a retarget passthrough. Change the class declaration line to

```cpp
class WavetableVoice : public SoundSource {
```

add `#include "SoundSource.h"` to its includes, add `override` to `setSampleRate`, `noteOff` (change to `void noteOff() override { env_.noteOff(); }`), `kill`, `isActive`, `render`, and `noteOn` — `noteOn`'s signature already matches. Add after `setParams`:

```cpp
    void setWavetable(const Wavetable* wavetable) { osc_.setTable(wavetable); }
```

(`render` keeps `noexcept` — that's a valid covariant addition on an override.)

- [ ] **Step 6: Build + full test suite (must stay 23/23 green — this is a refactor)**

```powershell
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

- [ ] **Step 7: Commit**

```powershell
git add plugin/engine/
git commit -m "refactor: SoundSource interface and retargetable wavetable voice"
```

---

### Task 2: Pitch detection (TDD)

**Files:**
- Create: `plugin/engine/PitchDetector.h`
- Create: `tests/engine/PitchDetectorTests.cpp`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1:** Add `engine/PitchDetectorTests.cpp` to the `EngineTests` source list.

- [ ] **Step 2: Failing tests in `tests/engine/PitchDetectorTests.cpp`:**

```cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <vector>
#include "engine/PitchDetector.h"

using namespace soundx::engine;
using Catch::Approx;

namespace {
std::vector<float> sine(double hz, double sr, std::size_t n) {
    std::vector<float> v(n);
    for (std::size_t i = 0; i < n; ++i)
        v[i] = float(std::sin(2.0 * std::numbers::pi * hz * double(i) / sr));
    return v;
}
} // namespace

TEST_CASE("detects the period of a pure sine") {
    const auto x = sine(440.0, 44100.0, 4096);
    const float period = detectPeriod(x.data(), x.size(), 44100.0);
    REQUIRE(period == Approx(44100.0 / 440.0).margin(1.5));
}

TEST_CASE("detects a low note") {
    const auto x = sine(82.4, 44100.0, 8192); // low E
    const float period = detectPeriod(x.data(), x.size(), 44100.0);
    REQUIRE(period == Approx(44100.0 / 82.4).margin(3.0));
}

TEST_CASE("detects a harmonically rich tone at the fundamental") {
    std::vector<float> x(4096);
    for (std::size_t i = 0; i < x.size(); ++i) {
        const double t = double(i) / 44100.0;
        x[i] = float(0.6 * std::sin(2.0 * std::numbers::pi * 220.0 * t)
                   + 0.3 * std::sin(2.0 * std::numbers::pi * 440.0 * t)
                   + 0.1 * std::sin(2.0 * std::numbers::pi * 660.0 * t));
    }
    const float period = detectPeriod(x.data(), x.size(), 44100.0);
    REQUIRE(period == Approx(44100.0 / 220.0).margin(2.0));
}

TEST_CASE("returns 0 for silence and noise") {
    std::vector<float> silence(4096, 0.0f);
    REQUIRE(detectPeriod(silence.data(), silence.size(), 44100.0) == 0.0f);

    std::vector<float> noise(4096);
    std::uint32_t seed = 1;
    for (auto& v : noise) {
        seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
        v = float(seed) / float(UINT32_MAX) * 2.0f - 1.0f;
    }
    REQUIRE(detectPeriod(noise.data(), noise.size(), 44100.0) == 0.0f);
}
```

- [ ] **Step 3:** Build → confirm compile FAILURE (missing header).

- [ ] **Step 4: Write `plugin/engine/PitchDetector.h`:**

```cpp
#pragma once
// JUCE-free. Offline normalized-autocorrelation period estimator.
// Not RT-safe by necessity (O(n * lags)) — call from import/background paths only.
#include <algorithm>
#include <cmath>
#include <cstddef>

namespace soundx::engine {

// Returns the fundamental period in samples, or 0 if no reliable pitch found.
inline float detectPeriod(const float* x, std::size_t n, double sampleRate,
                          float minHz = 50.0f, float maxHz = 2000.0f) {
    if (x == nullptr || n < 64 || sampleRate <= 0.0)
        return 0.0f;

    const auto minLag = std::max<std::size_t>(2, std::size_t(sampleRate / maxHz));
    const auto maxLag = std::min(n / 2, std::size_t(sampleRate / minHz));
    if (minLag >= maxLag)
        return 0.0f;

    double energy = 0.0;
    for (std::size_t i = 0; i < n; ++i)
        energy += double(x[i]) * x[i];
    if (energy < 1e-9)
        return 0.0f; // silence

    double bestR = 0.0;
    std::size_t bestLag = 0;
    for (auto lag = minLag; lag <= maxLag; ++lag) {
        double acc = 0.0, e1 = 0.0, e2 = 0.0;
        for (std::size_t i = 0; i + lag < n; ++i) {
            acc += double(x[i]) * x[i + lag];
            e1 += double(x[i]) * x[i];
            e2 += double(x[i + lag]) * x[i + lag];
        }
        const double norm = std::sqrt(e1 * e2);
        const double r = norm > 0.0 ? acc / norm : 0.0;
        if (r > bestR) {
            bestR = r;
            bestLag = lag;
        }
    }

    // Below this correlation the signal is effectively unpitched.
    constexpr double kVoicedThreshold = 0.8;
    if (bestR < kVoicedThreshold || bestLag == 0)
        return 0.0f;

    // Prefer the smallest lag whose correlation is nearly as good as the best —
    // guards against octave errors (picking 2x the true period).
    for (auto lag = minLag; lag < bestLag; ++lag) {
        double acc = 0.0, e1 = 0.0, e2 = 0.0;
        for (std::size_t i = 0; i + lag < n; ++i) {
            acc += double(x[i]) * x[i + lag];
            e1 += double(x[i]) * x[i];
            e2 += double(x[i + lag]) * x[i + lag];
        }
        const double norm = std::sqrt(e1 * e2);
        if (norm > 0.0 && acc / norm > bestR * 0.99) {
            bestLag = lag;
            break;
        }
    }
    return float(bestLag);
}

} // namespace soundx::engine
```

- [ ] **Step 5:** Build + ctest → all pass (23 + 4 = 27).

- [ ] **Step 6: Commit** — `feat: autocorrelation pitch detector for sample import`

---

### Task 3: Wavetable-from-sample extraction (TDD)

**Files:**
- Create: `plugin/engine/SampleImporter.h`
- Create: `tests/engine/ImporterTests.cpp`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1:** Add `engine/ImporterTests.cpp` to `EngineTests` sources.

- [ ] **Step 2: Failing tests in `tests/engine/ImporterTests.cpp`:**

```cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cmath>
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
    // peak near phase 0.25 relative to its own start is not guaranteed (phase
    // offset per slice), but the waveform must be strongly periodic with
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
```

- [ ] **Step 3:** Build → confirm FAILURE.

- [ ] **Step 4: Write `plugin/engine/SampleImporter.h`:**

```cpp
#pragma once
// JUCE-free. Offline conversion of a decoded sample into engine assets.
// Call from background/import threads only — allocates freely.
#include <algorithm>
#include <cmath>
#include <cstddef>
#include "PitchDetector.h"
#include "SampleData.h"
#include "Wavetable.h"

namespace soundx::engine {

struct SampleImporterDefaults {
    static constexpr std::size_t kNumSlices = 8;   // tables across the sample timeline
    static constexpr std::size_t kAnalysisWindow = 4096;
    static constexpr std::size_t kMinUsableSamples = 256;
};

namespace detail {

// Resample src[start .. start+srcLen) to exactly Wavetable::kTableSize points
// (linear interpolation), normalized to peak 1.
inline Wavetable::Table sliceToTable(const std::vector<float>& src, double start, double srcLen) {
    Wavetable::Table table(Wavetable::kTableSize, 0.0f);
    if (srcLen < 2.0 || src.empty())
        return table;
    const double maxIndex = double(src.size()) - 1.0;
    float peak = 0.0f;
    for (std::size_t i = 0; i < Wavetable::kTableSize; ++i) {
        const double pos = std::min(start + srcLen * double(i) / double(Wavetable::kTableSize), maxIndex);
        const auto i0 = std::size_t(pos);
        const auto i1 = std::min(i0 + 1, src.size() - 1);
        const float frac = float(pos - double(i0));
        table[i] = src[i0] * (1.0f - frac) + src[i1] * frac;
        peak = std::max(peak, std::abs(table[i]));
    }
    if (peak > 1e-6f)
        for (auto& v : table)
            v /= peak;
    return table;
}

} // namespace detail

// Builds a wavetable bank whose position axis scans the sample's evolution:
// 8 single-cycle slices taken at evenly spaced points through the file.
// Unpitched material falls back to raw kTableSize-sample slices; unusable
// input falls back to the factory sine/saw bank.
inline Wavetable makeWavetableFromSample(const SampleData& s) {
    const auto& x = s.samples;
    if (x.size() < SampleImporterDefaults::kMinUsableSamples)
        return Wavetable::makeSineSaw();

    // Detect pitch on a window from the middle of the file (steadier than the onset).
    const auto window = std::min(SampleImporterDefaults::kAnalysisWindow, x.size());
    const auto windowStart = (x.size() - window) / 2;
    const float period = detectPeriod(x.data() + windowStart, window, s.sourceSampleRate);

    const double sliceLen = (period >= 2.0f) ? double(period) : double(Wavetable::kTableSize);
    if (double(x.size()) < sliceLen + 1.0)
        return Wavetable::makeSineSaw();

    Wavetable wt;
    const double lastStart = double(x.size()) - sliceLen - 1.0;
    for (std::size_t k = 0; k < SampleImporterDefaults::kNumSlices; ++k) {
        const double start = (SampleImporterDefaults::kNumSlices == 1)
            ? 0.0
            : lastStart * double(k) / double(SampleImporterDefaults::kNumSlices - 1);
        wt.addTable(detail::sliceToTable(x, start, sliceLen));
    }
    return wt;
}

} // namespace soundx::engine
```

- [ ] **Step 5:** Build + ctest → all pass (27 + 3 = 30).

- [ ] **Step 6: Commit** — `feat: sample-to-wavetable import with pitch-synchronous slicing`

---

### Task 4: GranularVoice (TDD)

**Files:**
- Create: `plugin/engine/Rng.h`
- Create: `plugin/engine/GranularVoice.h`
- Create: `tests/engine/GranularTests.cpp`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1:** Add `engine/GranularTests.cpp` to `EngineTests` sources.

- [ ] **Step 2: Failing tests in `tests/engine/GranularTests.cpp`:**

```cpp
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
```

- [ ] **Step 3:** Build → confirm FAILURE.

- [ ] **Step 4: Write `plugin/engine/Rng.h`:**

```cpp
#pragma once
// JUCE-free xorshift32 — deterministic, allocation-free randomness for RT code.
#include <cstdint>

namespace soundx::engine {

class Rng {
public:
    explicit Rng(std::uint32_t seed = 0x9e3779b9u) : state_(seed != 0 ? seed : 1u) {}

    std::uint32_t next() noexcept {
        state_ ^= state_ << 13;
        state_ ^= state_ >> 17;
        state_ ^= state_ << 5;
        return state_;
    }

    // uniform in [0, 1)
    float next01() noexcept {
        return float(next() >> 8) * (1.0f / 16777216.0f);
    }

private:
    std::uint32_t state_;
};

} // namespace soundx::engine
```

- [ ] **Step 5: Write `plugin/engine/GranularVoice.h`:**

```cpp
#pragma once
// JUCE-free granular engine voice. render() is RT-safe: fixed grain pool,
// no allocation, no locks. setSample() must only be called while the voice
// is not rendering (the plugin swaps samples under suspendProcessing).
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <numbers>
#include "Adsr.h"
#include "Rng.h"
#include "SampleData.h"
#include "SoundSource.h"

namespace soundx::engine {

class GranularVoice : public SoundSource {
public:
    static constexpr std::size_t kMaxGrains = 32;
    static constexpr float kMinGrainMs = 5.0f, kMaxGrainMs = 500.0f;
    static constexpr float kMinDensityHz = 0.5f, kMaxDensityHz = 100.0f;

    void setSampleRate(double sampleRate) override {
        sampleRate_ = sampleRate;
        env_.setSampleRate(sampleRate);
    }

    void setSample(const SampleData* sample) { sample_ = sample; }

    void setEnvParams(float a, float d, float s, float r) { env_.setParams(a, d, s, r); }

    void setGrainParams(float grainSizeMs, float densityHz, float spray01, float position01) {
        grainSizeMs_ = std::clamp(grainSizeMs, kMinGrainMs, kMaxGrainMs);
        densityHz_ = std::clamp(densityHz, kMinDensityHz, kMaxDensityHz);
        spray_ = std::clamp(spray01, 0.0f, 1.0f);
        position_ = std::clamp(position01, 0.0f, 1.0f);
    }

    void noteOn(int midiNote, float velocity01) override {
        velocity_ = velocity01;
        // note 60 plays the sample at its recorded pitch
        rate_ = std::pow(2.0, double(midiNote - 60) / 12.0);
        samplesToNextGrain_ = 0; // spawn immediately
        env_.noteOn();
    }

    void noteOff() override { env_.noteOff(); }

    void kill() override {
        env_.reset();
        for (auto& g : grains_)
            g.active = false;
    }

    bool isActive() const override { return env_.isActive(); }

    void render(float* dest, int numSamples) override {
        if (!isActive() || sample_ == nullptr || sample_->samples.size() < 2 || sampleRate_ <= 0.0) {
            // keep envelope time flowing so a sample-less noteOn still expires
            for (int i = 0; i < numSamples; ++i)
                env_.nextLevel();
            return;
        }
        const auto& src = sample_->samples;
        const double srcLen = double(src.size());
        const double interOnset = sampleRate_ / double(densityHz_);

        for (int i = 0; i < numSamples; ++i) {
            const float envLevel = env_.nextLevel();
            if (envLevel <= 0.0f && !env_.isActive())
                break;

            // schedule: only while the note is held or releasing
            if (--samplesToNextGrain_ <= 0) {
                spawnGrain(srcLen);
                samplesToNextGrain_ = int(interOnset);
            }

            float mix = 0.0f;
            for (auto& g : grains_) {
                if (!g.active)
                    continue;
                // Hann window over grain age
                const float w = 0.5f * (1.0f - std::cos(2.0f * float(std::numbers::pi)
                                * float(g.age) / float(g.length)));
                const auto i0 = std::size_t(g.srcPos);
                const auto i1 = std::min(i0 + 1, src.size() - 1);
                const float frac = float(g.srcPos - double(i0));
                mix += (src[i0] * (1.0f - frac) + src[i1] * frac) * w;

                g.srcPos += g.rate;
                if (++g.age >= g.length || g.srcPos >= srcLen - 1.0)
                    g.active = false;
            }
            // 1/sqrt(maxOverlap) keeps dense clouds from clipping
            const float overlap = std::max(1.0f, densityHz_ * grainSizeMs_ * 0.001f);
            dest[i] += mix * envLevel * velocity_ / std::sqrt(overlap);
        }
    }

private:
    struct Grain {
        double srcPos = 0.0;
        double rate = 1.0;
        std::size_t age = 0, length = 0;
        bool active = false;
    };

    void spawnGrain(double srcLen) {
        for (auto& g : grains_) {
            if (g.active)
                continue;
            const double lengthSamples = double(grainSizeMs_) * 0.001 * sampleRate_;
            const double sprayOffset = double(spray_) * (rng_.next01() * 2.0f - 1.0f) * srcLen * 0.5;
            double start = double(position_) * (srcLen - lengthSamples - 1.0) + sprayOffset;
            start = std::clamp(start, 0.0, std::max(0.0, srcLen - 2.0));
            g.srcPos = start;
            g.rate = rate_;
            g.age = 0;
            g.length = std::max<std::size_t>(8, std::size_t(lengthSamples));
            g.active = true;
            return;
        }
        // pool exhausted: steal nothing, just skip this onset (graceful degradation)
    }

    std::array<Grain, kMaxGrains> grains_{};
    Adsr env_;
    Rng rng_;
    const SampleData* sample_ = nullptr;
    double sampleRate_ = 0.0;
    double rate_ = 1.0;
    float velocity_ = 0.0f;
    float grainSizeMs_ = 100.0f, densityHz_ = 30.0f, spray_ = 0.2f, position_ = 0.0f;
    int samplesToNextGrain_ = 0;
};

} // namespace soundx::engine
```

- [ ] **Step 6:** Build + ctest → all pass (30 + 5 = 35).

- [ ] **Step 7: Commit** — `feat: granular engine voice with fixed grain pool`

---

### Task 5: Plugin wiring — params, sample pipeline, mode dispatch (TDD via render test)

**Files:**
- Modify: `plugin/SynthVoice.h`
- Modify: `plugin/PluginProcessor.h`, `plugin/PluginProcessor.cpp`
- Modify: `tests/plugin/RenderTests.cpp`

- [ ] **Step 1: Add the failing render test** to `tests/plugin/RenderTests.cpp` (append; also add `#include "engine/SampleImporter.h"` and `#include <numbers>` at the top):

```cpp
TEST_CASE("granular mode plays a programmatically loaded sample") {
    juce::ScopedJuceInitialiser_GUI juceInit;
    SoundXAudioProcessor proc;
    proc.prepareToPlay(44100.0, 512);

    auto sample = std::make_shared<soundx::engine::SampleData>();
    sample->sourceSampleRate = 44100.0;
    sample->samples.resize(44100);
    for (std::size_t i = 0; i < sample->samples.size(); ++i)
        sample->samples[i] = float(std::sin(2.0 * std::numbers::pi * 440.0 * double(i) / 44100.0));
    proc.applySample(sample, "test-sine");

    auto* mode = proc.apvts().getParameter("mode");
    REQUIRE(mode != nullptr);
    mode->setValueNotifyingHost(1.0f); // Granular

    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.9f), 0);
    auto held = renderBlocks(proc, midi, 40, 512);
    REQUIRE(held.allFinite);
    REQUIRE(held.peakRms > 0.01f);
}

TEST_CASE("granular mode without a sample is silent but stable") {
    juce::ScopedJuceInitialiser_GUI juceInit;
    SoundXAudioProcessor proc;
    proc.prepareToPlay(44100.0, 512);
    proc.apvts().getParameter("mode")->setValueNotifyingHost(1.0f);

    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.9f), 0);
    auto held = renderBlocks(proc, midi, 10, 512);
    REQUIRE(held.allFinite);
    REQUIRE(held.peakRms < 1.0e-4f);
}
```

- [ ] **Step 2:** Build PluginRenderTests → confirm FAILURE (`applySample`/`mode` missing).

- [ ] **Step 3: Rewrite `plugin/SynthVoice.h`** (full replacement):

```cpp
#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <vector>
#include "engine/GranularVoice.h"
#include "engine/WavetableVoice.h"

class SynthSound : public juce::SynthesiserSound {
public:
    bool appliesToNote(int) override { return true; }
    bool appliesToChannel(int) override { return true; }
};

// Bridges juce::Synthesiser to the engine voices. Owns one voice per engine
// mode; `mode` chooses which one a new note plays on. Voices render additively
// so a note that started in one mode finishes its tail after a mode switch.
class SynthVoice : public juce::SynthesiserVoice {
public:
    enum class Mode { wavetable = 0, granular = 1 };

    explicit SynthVoice(const soundx::engine::Wavetable& wavetable) : wtVoice_(wavetable) {}

    // Pre-allocates the scratch buffer. Call from prepareToPlay, never from audio thread.
    void prepare(double sampleRate, int maxBlockSize) {
        wtVoice_.setSampleRate(sampleRate);
        granVoice_.setSampleRate(sampleRate);
        scratch_.assign(size_t(maxBlockSize), 0.0f);
    }

    void setMode(Mode m) { mode_ = m; }

    void setParams(float a, float d, float s, float r, float position,
                   float grainSizeMs, float densityHz, float spray) {
        wtVoice_.setParams(a, d, s, r, position);
        granVoice_.setEnvParams(a, d, s, r);
        granVoice_.setGrainParams(grainSizeMs, densityHz, spray, position);
    }

    // Safe only while the processor has rendering suspended.
    void setSources(const soundx::engine::Wavetable* wavetable,
                    const soundx::engine::SampleData* sample) {
        wtVoice_.setWavetable(wavetable);
        granVoice_.setSample(sample);
    }

    bool canPlaySound(juce::SynthesiserSound* sound) override {
        return dynamic_cast<SynthSound*>(sound) != nullptr;
    }

    void startNote(int midiNote, float velocity, juce::SynthesiserSound*, int) override {
        activeSource_ = (mode_ == Mode::granular)
            ? static_cast<soundx::engine::SoundSource*>(&granVoice_)
            : static_cast<soundx::engine::SoundSource*>(&wtVoice_);
        activeSource_->noteOn(midiNote, velocity);
    }

    void stopNote(float, bool allowTailOff) override {
        if (activeSource_ == nullptr) {
            clearCurrentNote();
            return;
        }
        if (allowTailOff) {
            activeSource_->noteOff();
        } else {
            activeSource_->kill();
            clearCurrentNote();
        }
    }

    void pitchWheelMoved(int) override {}
    void controllerMoved(int, int) override {}

    void renderNextBlock(juce::AudioBuffer<float>& output, int startSample, int numSamples) override {
        if (activeSource_ == nullptr || !activeSource_->isActive()) {
            clearCurrentNote();
            return;
        }
        // Defensive: never trust the host to honor the prepared block size.
        numSamples = std::min(numSamples, int(scratch_.size()));
        std::fill_n(scratch_.data(), size_t(numSamples), 0.0f);
        activeSource_->render(scratch_.data(), numSamples);
        for (int ch = 0; ch < output.getNumChannels(); ++ch)
            output.addFrom(ch, startSample, scratch_.data(), numSamples);
        if (!activeSource_->isActive())
            clearCurrentNote();
    }

private:
    soundx::engine::WavetableVoice wtVoice_;
    soundx::engine::GranularVoice granVoice_;
    soundx::engine::SoundSource* activeSource_ = nullptr;
    Mode mode_ = Mode::wavetable;
    std::vector<float> scratch_;
};
```

- [ ] **Step 4: Update `plugin/PluginProcessor.h`** — add includes `#include <memory>` and `#include "engine/SampleData.h"`; add to the public section:

```cpp
    // Swaps in a new sample + derived wavetable bank. Called from the message
    // thread (file import) or tests; suspends processing during the swap.
    void applySample(std::shared_ptr<const soundx::engine::SampleData> sample,
                     const juce::String& name);

    // Kicks off async decode+import of an audio file (background thread).
    void loadSampleFile(const juce::File& file);

    juce::String currentSampleName() const { return sampleName_; }
```

and to the private section:

```cpp
    void rebindVoiceSources();

    std::shared_ptr<const soundx::engine::SampleData> sample_;       // grain source
    std::unique_ptr<soundx::engine::Wavetable> importedWavetable_;   // from sample
    juce::String sampleName_;
    juce::ThreadPool importPool_{1};

    std::atomic<float>* mode_ = nullptr;
    std::atomic<float>* grainsize_ = nullptr;
    std::atomic<float>* density_ = nullptr;
    std::atomic<float>* spray_ = nullptr;
```

- [ ] **Step 5: Update `plugin/PluginProcessor.cpp`:**

Add includes at top: `#include "engine/SampleImporter.h"`.

In `createParameterLayout()`, add before `return layout;`:

```cpp
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"mode", 1}, "Engine Mode",
        juce::StringArray{"Wavetable", "Granular"}, 0));
    layout.add(std::make_unique<P>(juce::ParameterID{"grainsize", 1}, "Grain Size",
                                   juce::NormalisableRange<float>(5.0f, 500.0f, 0.0f, 0.4f), 100.0f));
    layout.add(std::make_unique<P>(juce::ParameterID{"density", 1}, "Density",
                                   juce::NormalisableRange<float>(0.5f, 100.0f, 0.0f, 0.4f), 30.0f));
    layout.add(std::make_unique<P>(juce::ParameterID{"spray", 1}, "Spray",
                                   juce::NormalisableRange<float>(0.0f, 1.0f), 0.2f));
```

In the constructor body, add:

```cpp
    mode_ = apvts_.getRawParameterValue("mode");
    grainsize_ = apvts_.getRawParameterValue("grainsize");
    density_ = apvts_.getRawParameterValue("density");
    spray_ = apvts_.getRawParameterValue("spray");
```

Replace the parameter-push block in `processBlock` with:

```cpp
    const float a = attack_->load(), d = decay_->load(), s = sustain_->load(),
                r = release_->load(), pos = position_->load();
    const float gsize = grainsize_->load(), dens = density_->load(), spr = spray_->load();
    const auto mode = mode_->load() >= 0.5f ? SynthVoice::Mode::granular
                                            : SynthVoice::Mode::wavetable;
    for (int i = 0; i < synth_.getNumVoices(); ++i)
        if (auto* v = dynamic_cast<SynthVoice*>(synth_.getVoice(i))) {
            v->setMode(mode);
            v->setParams(a, d, s, r, pos, gsize, dens, spr);
        }
```

Add the new methods at the end (before `createPluginFilter`):

```cpp
void SoundXAudioProcessor::rebindVoiceSources() {
    const auto* wt = importedWavetable_ != nullptr ? importedWavetable_.get() : &wavetable_;
    for (int i = 0; i < synth_.getNumVoices(); ++i)
        if (auto* v = dynamic_cast<SynthVoice*>(synth_.getVoice(i)))
            v->setSources(wt, sample_.get());
}

void SoundXAudioProcessor::applySample(std::shared_ptr<const soundx::engine::SampleData> sample,
                                       const juce::String& name) {
    auto imported = std::make_unique<soundx::engine::Wavetable>(
        soundx::engine::makeWavetableFromSample(*sample));

    suspendProcessing(true);
    sample_ = std::move(sample);
    importedWavetable_ = std::move(imported);
    sampleName_ = name;
    rebindVoiceSources();
    suspendProcessing(false);
}

void SoundXAudioProcessor::loadSampleFile(const juce::File& file) {
    importPool_.addJob([this, file] {
        juce::AudioFormatManager formats;
        formats.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
        if (reader == nullptr)
            return; // unsupported/corrupt: silently keep the current sample

        constexpr juce::int64 kMaxSeconds = 30;
        const auto numSamples = juce::jmin(reader->lengthInSamples,
                                           juce::int64(reader->sampleRate) * kMaxSeconds);
        if (numSamples < 2)
            return;

        juce::AudioBuffer<float> buffer(int(reader->numChannels), int(numSamples));
        reader->read(&buffer, 0, int(numSamples), 0, true, true);

        auto data = std::make_shared<soundx::engine::SampleData>();
        data->sourceSampleRate = reader->sampleRate;
        data->samples.resize(size_t(numSamples), 0.0f);
        const float channelScale = 1.0f / float(buffer.getNumChannels());
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch) {
            const float* in = buffer.getReadPointer(ch);
            for (int i = 0; i < int(numSamples); ++i)
                data->samples[size_t(i)] += in[i] * channelScale;
        }
        float peak = 0.0f;
        for (float v : data->samples)
            peak = std::max(peak, std::abs(v));
        if (peak > 1.0f)
            for (auto& v : data->samples)
                v /= peak;

        juce::MessageManager::callAsync([this, data, name = file.getFileName()] {
            applySample(data, name);
        });
    });
}
```

In `prepareToPlay`, after the voice-prepare loop, add `rebindVoiceSources();` (re-pins sources after voice buffers reset). In the constructor, after the addVoice loop, also call `rebindVoiceSources();`.

- [ ] **Step 6:** Build everything + run full suite → expect 37/37 (35 engine + old 2 + new 2 plugin = 39 actually: 35 engine tests + 4 plugin tests).

```powershell
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

- [ ] **Step 7: Commit** — `feat: granular mode, sample import pipeline, engine mode dispatch`

---

### Task 6: Editor — drag-and-drop, mode selector, granular knobs

**Files:**
- Modify: `plugin/PluginEditor.h`, `plugin/PluginEditor.cpp`

- [ ] **Step 1: Update `plugin/PluginEditor.h`** (full replacement):

```cpp
#pragma once
#include <array>
#include "PluginProcessor.h"

class SoundXAudioProcessorEditor : public juce::AudioProcessorEditor,
                                   public juce::FileDragAndDropTarget {
public:
    explicit SoundXAudioProcessorEditor(SoundXAudioProcessor&);
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseUp(const juce::MouseEvent&) override;

    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;
    void fileDragEnter(const juce::StringArray&, int, int) override;
    void fileDragExit(const juce::StringArray&) override;

private:
    static constexpr int kNumSliders = 9;
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    struct LabeledSlider {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<SliderAttachment> attachment;
    };

    SoundXAudioProcessor& processor_;
    std::array<LabeledSlider, kNumSliders> sliders_;
    juce::ComboBox modeBox_;
    std::unique_ptr<ComboAttachment> modeAttachment_;
    bool dragHover_ = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SoundXAudioProcessorEditor)
};
```

- [ ] **Step 2: Update `plugin/PluginEditor.cpp`** (full replacement):

```cpp
#include "PluginEditor.h"

#if JUCE_WINDOWS
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
 #endif
 #include <windows.h>
#endif

namespace {
constexpr auto kBackground = 0xff02090c;
constexpr auto kAccent = 0xff22d3ee;
constexpr auto kDim = 0xff0e3a40;

constexpr std::array<const char*, 9> kParamIds = {"gain", "attack", "decay", "sustain",
                                                  "release", "position", "grainsize",
                                                  "density", "spray"};
constexpr std::array<const char*, 9> kParamNames = {"GAIN", "ATK", "DEC", "SUS",
                                                    "REL", "POS", "GRAIN", "DENS", "SPRAY"};

bool isSupportedAudioFile(const juce::String& path) {
    return path.endsWithIgnoreCase(".wav") || path.endsWithIgnoreCase(".aif")
        || path.endsWithIgnoreCase(".aiff") || path.endsWithIgnoreCase(".flac")
        || path.endsWithIgnoreCase(".ogg") || path.endsWithIgnoreCase(".mp3");
}
} // namespace

SoundXAudioProcessorEditor::SoundXAudioProcessorEditor(SoundXAudioProcessor& p)
    : AudioProcessorEditor(&p), processor_(p) {
    for (int i = 0; i < kNumSliders; ++i) {
        auto& s = sliders_[size_t(i)];
        s.slider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
        s.slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 64, 16);
        s.slider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(kAccent));
        s.slider.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(kDim));
        s.slider.setColour(juce::Slider::textBoxTextColourId, juce::Colour(kAccent));
        s.slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(kDim));
        // Let FL Studio's typing keyboard keep working while the editor is focused:
        // sliders grab keyboard focus by default and would swallow note keys.
        s.slider.setWantsKeyboardFocus(false);
        addAndMakeVisible(s.slider);

        s.label.setText(kParamNames[size_t(i)], juce::dontSendNotification);
        s.label.setJustificationType(juce::Justification::centred);
        s.label.setColour(juce::Label::textColourId, juce::Colour(kAccent));
        s.label.setFont(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 12.0f, juce::Font::plain));
        addAndMakeVisible(s.label);

        s.attachment = std::make_unique<SliderAttachment>(processor_.apvts(), kParamIds[size_t(i)], s.slider);
    }

    modeBox_.addItem("WAVETABLE", 1);
    modeBox_.addItem("GRANULAR", 2);
    modeBox_.setColour(juce::ComboBox::backgroundColourId, juce::Colour(kBackground));
    modeBox_.setColour(juce::ComboBox::textColourId, juce::Colour(kAccent));
    modeBox_.setColour(juce::ComboBox::outlineColourId, juce::Colour(kDim));
    modeBox_.setColour(juce::ComboBox::arrowColourId, juce::Colour(kAccent));
    modeBox_.setWantsKeyboardFocus(false);
    addAndMakeVisible(modeBox_);
    modeAttachment_ = std::make_unique<ComboAttachment>(processor_.apvts(), "mode", modeBox_);

    setWantsKeyboardFocus(false);
    // Hear about clicks on every child so we can hand keyboard focus back to the host.
    addMouseListener(this, true);
    setResizable(true, true);
    setResizeLimits(640, 360, 1600, 900);
    setSize(860, 460);
}

void SoundXAudioProcessorEditor::mouseUp(const juce::MouseEvent& e) {
    // Keep FL Studio's typing keyboard alive: clicking our UI moves OS keyboard
    // focus onto the plugin window, so hand it straight back to the host's
    // wrapper window. Skip text fields so typing values into knobs still works.
    if (dynamic_cast<juce::TextEditor*>(e.eventComponent) != nullptr
        || dynamic_cast<juce::Label*>(e.eventComponent) != nullptr)
        return;
#if JUCE_WINDOWS
    if (auto* peer = getPeer())
        if (HWND parent = ::GetParent(static_cast<HWND>(peer->getNativeHandle())))
            ::SetFocus(parent);
#endif
}

bool SoundXAudioProcessorEditor::isInterestedInFileDrag(const juce::StringArray& files) {
    for (const auto& f : files)
        if (isSupportedAudioFile(f))
            return true;
    return false;
}

void SoundXAudioProcessorEditor::fileDragEnter(const juce::StringArray&, int, int) {
    dragHover_ = true;
    repaint();
}

void SoundXAudioProcessorEditor::fileDragExit(const juce::StringArray&) {
    dragHover_ = false;
    repaint();
}

void SoundXAudioProcessorEditor::filesDropped(const juce::StringArray& files, int, int) {
    dragHover_ = false;
    for (const auto& f : files)
        if (isSupportedAudioFile(f)) {
            processor_.loadSampleFile(juce::File(f));
            break;
        }
    repaint();
}

void SoundXAudioProcessorEditor::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(kBackground));
    g.setColour(juce::Colour(kAccent));
    g.setFont(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 22.0f, juce::Font::plain));
    g.drawText("SOUNDX::ENGINE", getLocalBounds().removeFromTop(48), juce::Justification::centred);

    g.setFont(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 12.0f, juce::Font::plain));
    const auto sampleName = processor_.currentSampleName();
    const auto sampleText = sampleName.isEmpty()
        ? juce::String("DRAG AUDIO FILE HERE — plays in both engines")
        : "SAMPLE: " + sampleName;
    g.setColour(juce::Colour(dragHover_ ? kAccent : kDim).brighter(dragHover_ ? 0.4f : 0.0f));
    auto sampleZone = getLocalBounds().reduced(24).removeFromTop(72).removeFromBottom(24);
    g.drawRect(sampleZone, 1);
    g.setColour(juce::Colour(kAccent).withAlpha(dragHover_ ? 1.0f : 0.7f));
    g.drawText(sampleText, sampleZone, juce::Justification::centred);

    g.setColour(juce::Colour(kDim));
    g.drawRect(getLocalBounds().reduced(8), 1);
}

void SoundXAudioProcessorEditor::resized() {
    auto area = getLocalBounds().reduced(24);
    area.removeFromTop(48); // title
    auto topRow = area.removeFromTop(24);
    modeBox_.setBounds(topRow.removeFromLeft(160));
    area.removeFromTop(28); // sample drop zone (painted)

    const int cell = area.getWidth() / kNumSliders;
    for (auto& s : sliders_) {
        auto col = area.removeFromLeft(cell).reduced(6);
        s.label.setBounds(col.removeFromTop(18));
        s.slider.setBounds(col.withHeight(juce::jmin(col.getHeight(), 130)));
    }
}
```

- [ ] **Step 3:** Build + full suite green; launch Standalone and verify visually: mode combo top-left, drop-zone hint, 9 knobs.

- [ ] **Step 4: Commit** — `feat: sample drag-and-drop, engine mode selector, granular knobs`

---

### Task 7: Validate, document, merge

- [ ] **Step 1:** Run pluginval at strictness 10 on the rebuilt VST3 — must end `SUCCESS`.
- [ ] **Step 2:** Update README feature list (granular mode + drag-and-drop sample import).
- [ ] **Step 3:** Merge:

```powershell
git checkout main
git merge --no-ff feat/plan2a-granular-import -m "merge: plan 2a - granular engine + sample import"
```

---

## Self-review notes

- **Spec coverage (2a scope):** SoundSource interface ✓; sample → wavetable bank + grain source (2 of 3 representations; spectral = Plan 2b) ✓; import off the audio thread ✓ (ThreadPool decode + import, swap under suspendProcessing); granular params (size/density/spray + position reuse) ✓; reject unsupported files without crash ✓ (reader == nullptr → keep current); 30s cap ✓; RT rules: grain pool fixed, no allocation in render ✓.
- **Type consistency:** `applySample(shared_ptr<const SampleData>, String)`, `loadSampleFile(File)`, `SynthVoice::setMode/setParams(8 args)/setSources`, `GranularVoice::setEnvParams/setGrainParams/setSample`, `makeWavetableFromSample(const SampleData&)`, `detectPeriod(ptr, n, sr)` — verified consistent across tasks.
- **Known risks:** `suspendProcessing` audibly gaps playback during sample swap (acceptable v1; lock-free swap is a later refinement). Mode switch mid-note keeps old tail by design (voices render via activeSource_ until release).
