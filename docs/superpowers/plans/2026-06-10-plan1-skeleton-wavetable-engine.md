# SoundX Plan 1: Plugin Skeleton + Wavetable Engine

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A buildable VST3 synth named SoundX that plays a wavetable oscillator with ADSR envelope in FL Studio, backed by Catch2 unit tests, an offline render test, and CI with pluginval.

**Architecture:** JUCE 8 plugin (CMake + FetchContent) with a JUCE-free, header-only DSP core under `plugin/engine/` (namespace `soundx::engine`) so all DSP is testable without a host. A thin `juce::Synthesiser` wrapper bridges the engine to the plugin. Tests build as console apps: `EngineTests` (no JUCE) and `PluginRenderTests` (links the plugin's shared code).

**Tech Stack:** C++20, JUCE 8.0.4, CMake ≥3.24, Catch2 v3, Visual Studio 2022, pluginval, GitHub Actions.

**Spec:** `docs/superpowers/specs/2026-06-10-soundx-synth-design.md`

---

## Roadmap context (where this plan sits)

| Plan | Delivers | Status |
|---|---|---|
| **1. Skeleton + wavetable engine** | **Buildable, playable VST3 + test/CI foundation** | **this plan** |
| 2. Granular + spectral engines | `SoundSource` interface, `SampleImporter` (one sample → 3 engines) | later |
| 3. Morph + modulation | `MorphController` (bit-exact endpoints), `ModMatrix`, LFOs/envs/macros | later |
| 4. FX rack | distortion, chorus, delay, reverb, multiband comp | later |
| 5. HUD UI + Harmonic Orbits | OpenGL 3D/4D visualizer, `analysis/` (partials, consonance) | later |
| 6. Presets + factory + release | preset JSON, Python factory, 50+ presets, installer/release | later |

## File structure created by this plan

```
SoundX/
├── CMakeLists.txt                      # root: JUCE fetch, subdirs, testing
├── plugin/
│   ├── CMakeLists.txt                  # juce_add_plugin target "SoundX"
│   ├── engine/                         # JUCE-FREE header-only DSP (soundx::engine)
│   │   ├── Wavetable.h                 # table bank + interpolated read
│   │   ├── WavetableOscillator.h       # phase accumulator over a Wavetable
│   │   ├── Adsr.h                      # linear ADSR envelope
│   │   └── WavetableVoice.h            # osc + env + velocity -> buffer
│   ├── SynthVoice.h                    # juce::SynthesiserVoice bridge + SynthSound
│   ├── PluginProcessor.h / .cpp        # APVTS params, synth, state save/load
│   └── PluginEditor.h / .cpp           # minimal HUD-flavored controls
├── tests/
│   ├── CMakeLists.txt                  # Catch2 fetch, EngineTests, PluginRenderTests
│   ├── engine/
│   │   ├── WavetableTests.cpp
│   │   ├── OscillatorTests.cpp
│   │   ├── AdsrTests.cpp
│   │   └── VoiceTests.cpp
│   └── plugin/
│       └── RenderTests.cpp             # headless processor render + state recall
└── .github/workflows/build.yml         # Windows build + ctest + pluginval
```

Engine rules (from spec, enforced throughout): `plugin/engine/*` includes **no JUCE headers**, does **no allocation in render paths** (allocation only in setup/factory functions), and is fully deterministic.

---

### Task 1: CMake skeleton + minimal silent plugin builds as VST3

**Files:**
- Create: `CMakeLists.txt`
- Create: `plugin/CMakeLists.txt`
- Create: `plugin/PluginProcessor.h`
- Create: `plugin/PluginProcessor.cpp`
- Create: `plugin/PluginEditor.h`
- Create: `plugin/PluginEditor.cpp`

- [ ] **Step 1: Create a feature branch**

```powershell
git checkout -b feat/plan1-skeleton-wavetable
```

- [ ] **Step 2: Write root `CMakeLists.txt`**

```cmake
cmake_minimum_required(VERSION 3.24)
project(SoundX VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

add_subdirectory(plugin)

enable_testing()
add_subdirectory(tests)
```

Note: `tests/` doesn't exist until Task 2 — create the directory with an empty `CMakeLists.txt` now so configure succeeds:

```powershell
New-Item -ItemType Directory -Force tests | Out-Null
New-Item -ItemType File tests/CMakeLists.txt | Out-Null
```

(If JUCE tag `8.0.4` fails to fetch, use the newest `8.x` tag from https://github.com/juce-framework/JUCE/tags.)

- [ ] **Step 3: Write `plugin/CMakeLists.txt`**

```cmake
juce_add_plugin(SoundX
    COMPANY_NAME "SoundX Audio"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Sndx
    PLUGIN_CODE Sx01
    FORMATS VST3 Standalone
    PRODUCT_NAME "SoundX")

target_sources(SoundX PRIVATE
    PluginProcessor.cpp
    PluginEditor.cpp)

target_include_directories(SoundX PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_definitions(SoundX PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(SoundX
    PUBLIC
        juce::juce_audio_utils
    PRIVATE
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
```

(`PUBLIC` on includes/definitions/`juce_audio_utils` is deliberate — `PluginRenderTests` links target `SoundX` in Task 7 and needs them propagated.)

- [ ] **Step 4: Write `plugin/PluginProcessor.h`** (silent passthrough for now; synth wired in Task 7)

```cpp
#pragma once
#include <juce_audio_utils/juce_audio_utils.h>

class SoundXAudioProcessor : public juce::AudioProcessor {
public:
    SoundXAudioProcessor();

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "SoundX"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SoundXAudioProcessor)
};
```

- [ ] **Step 5: Write `plugin/PluginProcessor.cpp`**

```cpp
#include "PluginProcessor.h"
#include "PluginEditor.h"

SoundXAudioProcessor::SoundXAudioProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)) {}

void SoundXAudioProcessor::prepareToPlay(double, int) {}

void SoundXAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
}

bool SoundXAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        || layouts.getMainOutputChannelSet() == juce::AudioChannelSet::mono();
}

juce::AudioProcessorEditor* SoundXAudioProcessor::createEditor() {
    return new SoundXAudioProcessorEditor(*this);
}

void SoundXAudioProcessor::getStateInformation(juce::MemoryBlock&) {}
void SoundXAudioProcessor::setStateInformation(const void*, int) {}

// JUCE plugin entry point
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new SoundXAudioProcessor();
}
```

- [ ] **Step 6: Write `plugin/PluginEditor.h`**

```cpp
#pragma once
#include "PluginProcessor.h"

class SoundXAudioProcessorEditor : public juce::AudioProcessorEditor {
public:
    explicit SoundXAudioProcessorEditor(SoundXAudioProcessor&);
    void paint(juce::Graphics&) override;
    void resized() override {}

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SoundXAudioProcessorEditor)
};
```

- [ ] **Step 7: Write `plugin/PluginEditor.cpp`**

```cpp
#include "PluginEditor.h"

SoundXAudioProcessorEditor::SoundXAudioProcessorEditor(SoundXAudioProcessor& p)
    : AudioProcessorEditor(&p) {
    setSize(720, 420);
}

void SoundXAudioProcessorEditor::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xff02090c));
    g.setColour(juce::Colour(0xff22d3ee));
    g.setFont(juce::FontOptions(20.0f));
    g.drawText("SOUNDX::ENGINE", getLocalBounds(), juce::Justification::centred);
}
```

- [ ] **Step 8: Configure and build (first run downloads JUCE — takes a few minutes)**

```powershell
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

Expected: build succeeds; `build\plugin\SoundX_artefacts\Release\VST3\SoundX.vst3` and `build\plugin\SoundX_artefacts\Release\Standalone\SoundX.exe` exist.

- [ ] **Step 9: Launch the standalone to verify the window opens**

```powershell
Start-Process "build\plugin\SoundX_artefacts\Release\Standalone\SoundX.exe"
```

Expected: dark window with cyan "SOUNDX::ENGINE" text. Close it.

- [ ] **Step 10: Commit**

```powershell
git add CMakeLists.txt plugin/ tests/CMakeLists.txt
git commit -m "feat: JUCE plugin skeleton builds VST3 and standalone"
```

---

### Task 2: Catch2 test harness

**Files:**
- Modify: `tests/CMakeLists.txt` (currently empty)
- Create: `tests/engine/WavetableTests.cpp` (sanity test only; real tests in Task 3)

- [ ] **Step 1: Write `tests/CMakeLists.txt`**

```cmake
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.7.1
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(Catch2)
list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)

add_executable(EngineTests
    engine/WavetableTests.cpp)
target_include_directories(EngineTests PRIVATE ${CMAKE_SOURCE_DIR}/plugin)
target_compile_features(EngineTests PRIVATE cxx_std_20)
target_link_libraries(EngineTests PRIVATE Catch2::Catch2WithMain)
catch_discover_tests(EngineTests)
```

- [ ] **Step 2: Write a sanity test in `tests/engine/WavetableTests.cpp`**

```cpp
#include <catch2/catch_test_macros.hpp>

TEST_CASE("test harness runs") {
    REQUIRE(1 + 1 == 2);
}
```

- [ ] **Step 3: Build and run tests**

```powershell
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target EngineTests
ctest --test-dir build -C Release --output-on-failure
```

Expected: `100% tests passed, 0 tests failed out of 1`.

- [ ] **Step 4: Commit**

```powershell
git add tests/
git commit -m "test: add Catch2 harness with EngineTests target"
```

---

### Task 3: `soundx::engine::Wavetable` — table bank with 2D interpolation

**Files:**
- Create: `plugin/engine/Wavetable.h`
- Modify: `tests/engine/WavetableTests.cpp` (replace sanity test)

- [ ] **Step 1: Write the failing tests** (replace the whole file)

```cpp
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
```

- [ ] **Step 2: Run tests to verify they fail**

```powershell
cmake --build build --config Release --target EngineTests
```

Expected: compile FAILURE — `engine/Wavetable.h: No such file or directory`.

- [ ] **Step 3: Write `plugin/engine/Wavetable.h`**

```cpp
#pragma once
// JUCE-free. Allocation only in addTable()/makeSineSaw() — sample() is RT-safe.
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <vector>

namespace soundx::engine {

class Wavetable {
public:
    static constexpr std::size_t kTableSize = 2048;
    using Table = std::vector<float>;

    void addTable(Table samples) {
        assert(samples.size() == kTableSize);
        tables_.push_back(std::move(samples));
    }

    std::size_t numTables() const { return tables_.size(); }

    // phase in [0,1), position in [0,1] across the bank
    float sample(float phase, float position) const {
        if (tables_.empty())
            return 0.0f;
        const float scaled = std::clamp(position, 0.0f, 1.0f) * float(tables_.size() - 1);
        const auto low = std::min(std::size_t(scaled), tables_.size() - 1);
        const auto high = std::min(low + 1, tables_.size() - 1);
        const float tableFrac = scaled - float(low);
        return readTable(tables_[low], phase) * (1.0f - tableFrac)
             + readTable(tables_[high], phase) * tableFrac;
    }

    // Table 0: pure sine. Table 1: band-limited saw (64 harmonics).
    static Wavetable makeSineSaw() {
        constexpr double twoPi = 2.0 * std::numbers::pi;
        Wavetable wt;
        Table sine(kTableSize), saw(kTableSize, 0.0f);
        for (std::size_t i = 0; i < kTableSize; ++i) {
            const double x = double(i) / double(kTableSize);
            sine[i] = float(std::sin(twoPi * x));
            double s = 0.0;
            for (int k = 1; k <= 64; ++k)
                s += ((k % 2 == 1) ? 1.0 : -1.0) * std::sin(twoPi * k * x) / double(k);
            saw[i] = float(s * (2.0 / std::numbers::pi));
        }
        wt.addTable(std::move(sine));
        wt.addTable(std::move(saw));
        return wt;
    }

private:
    static float readTable(const Table& t, float phase) {
        const float idx = (phase - std::floor(phase)) * float(kTableSize);
        const auto i0 = std::size_t(idx) % kTableSize;
        const auto i1 = (i0 + 1) % kTableSize;
        const float frac = idx - std::floor(idx);
        return t[i0] * (1.0f - frac) + t[i1] * frac;
    }

    std::vector<Table> tables_;
};

} // namespace soundx::engine
```

- [ ] **Step 4: Run tests to verify they pass**

```powershell
cmake --build build --config Release --target EngineTests
ctest --test-dir build -C Release --output-on-failure
```

Expected: all tests PASS.

- [ ] **Step 5: Commit**

```powershell
git add plugin/engine/Wavetable.h tests/engine/WavetableTests.cpp
git commit -m "feat: wavetable bank with phase and position interpolation"
```

---

### Task 4: `soundx::engine::WavetableOscillator`

**Files:**
- Create: `plugin/engine/WavetableOscillator.h`
- Create: `tests/engine/OscillatorTests.cpp`
- Modify: `tests/CMakeLists.txt` (add source)

- [ ] **Step 1: Add `engine/OscillatorTests.cpp` to the `EngineTests` sources in `tests/CMakeLists.txt`**

```cmake
add_executable(EngineTests
    engine/WavetableTests.cpp
    engine/OscillatorTests.cpp)
```

- [ ] **Step 2: Write the failing tests in `tests/engine/OscillatorTests.cpp`**

```cpp
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
```

- [ ] **Step 3: Run tests to verify they fail**

```powershell
cmake --build build --config Release --target EngineTests
```

Expected: compile FAILURE — `engine/WavetableOscillator.h: No such file`.

- [ ] **Step 4: Write `plugin/engine/WavetableOscillator.h`**

```cpp
#pragma once
// JUCE-free. nextSample() is RT-safe (no allocation, no locks).
#include <algorithm>
#include "Wavetable.h"

namespace soundx::engine {

class WavetableOscillator {
public:
    explicit WavetableOscillator(const Wavetable& wavetable) : table_(&wavetable) {}

    void setSampleRate(double sampleRate) {
        sampleRate_ = sampleRate;
        updateIncrement();
    }
    void setFrequency(float hz) {
        frequency_ = hz;
        updateIncrement();
    }
    void setPosition(float position01) { position_ = std::clamp(position01, 0.0f, 1.0f); }
    void reset() { phase_ = 0.0f; }

    float nextSample() {
        const float out = table_->sample(phase_, position_);
        phase_ += increment_;
        if (phase_ >= 1.0f)
            phase_ -= 1.0f;
        return out;
    }

private:
    void updateIncrement() {
        increment_ = sampleRate_ > 0.0 ? float(frequency_ / sampleRate_) : 0.0f;
    }

    const Wavetable* table_;
    double sampleRate_ = 0.0;
    float frequency_ = 0.0f;
    float increment_ = 0.0f;
    float phase_ = 0.0f;
    float position_ = 0.0f;
};

} // namespace soundx::engine
```

- [ ] **Step 5: Run tests to verify they pass**

```powershell
cmake --build build --config Release --target EngineTests
ctest --test-dir build -C Release --output-on-failure
```

Expected: all tests PASS.

- [ ] **Step 6: Commit**

```powershell
git add plugin/engine/WavetableOscillator.h tests/engine/OscillatorTests.cpp tests/CMakeLists.txt
git commit -m "feat: wavetable oscillator with phase accumulator and position morph"
```

---

### Task 5: `soundx::engine::Adsr`

**Files:**
- Create: `plugin/engine/Adsr.h`
- Create: `tests/engine/AdsrTests.cpp`
- Modify: `tests/CMakeLists.txt` (add source)

- [ ] **Step 1: Add `engine/AdsrTests.cpp` to `EngineTests` sources in `tests/CMakeLists.txt`**

```cmake
add_executable(EngineTests
    engine/WavetableTests.cpp
    engine/OscillatorTests.cpp
    engine/AdsrTests.cpp)
```

- [ ] **Step 2: Write the failing tests in `tests/engine/AdsrTests.cpp`**

```cpp
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
```

- [ ] **Step 3: Run tests to verify they fail**

```powershell
cmake --build build --config Release --target EngineTests
```

Expected: compile FAILURE — `engine/Adsr.h: No such file`.

- [ ] **Step 4: Write `plugin/engine/Adsr.h`**

```cpp
#pragma once
// JUCE-free linear ADSR. nextLevel() is RT-safe.
#include <algorithm>

namespace soundx::engine {

class Adsr {
public:
    void setSampleRate(double sampleRate) { sampleRate_ = sampleRate; }

    void setParams(float attackSeconds, float decaySeconds, float sustain01, float releaseSeconds) {
        attackS_ = std::max(attackSeconds, 0.0f);
        decayS_ = std::max(decaySeconds, 0.0f);
        sustain_ = std::clamp(sustain01, 0.0f, 1.0f);
        releaseS_ = std::max(releaseSeconds, 0.0f);
    }

    void noteOn() { stage_ = Stage::Attack; }

    void noteOff() {
        if (stage_ != Stage::Idle) {
            releaseStart_ = level_;
            stage_ = Stage::Release;
        }
    }

    void reset() {
        stage_ = Stage::Idle;
        level_ = 0.0f;
    }

    bool isActive() const { return stage_ != Stage::Idle; }

    float nextLevel() {
        switch (stage_) {
        case Stage::Attack:
            level_ += perSample(attackS_); // 0 -> 1 in attackS_
            if (level_ >= 1.0f) {
                level_ = 1.0f;
                stage_ = Stage::Decay;
            }
            break;
        case Stage::Decay:
            level_ -= perSample(decayS_) * (1.0f - sustain_); // 1 -> sustain in decayS_
            if (level_ <= sustain_) {
                level_ = sustain_;
                stage_ = Stage::Sustain;
            }
            break;
        case Stage::Sustain:
            level_ = sustain_;
            break;
        case Stage::Release:
            level_ -= perSample(releaseS_) * releaseStart_; // releaseStart_ -> 0 in releaseS_
            if (level_ <= 0.0f) {
                level_ = 0.0f;
                stage_ = Stage::Idle;
            }
            break;
        case Stage::Idle:
            level_ = 0.0f;
            break;
        }
        return level_;
    }

private:
    enum class Stage { Idle, Attack, Decay, Sustain, Release };

    // amount of a 0->1 ramp covered per sample for a stage lasting `seconds`
    float perSample(float seconds) const {
        return (seconds <= 0.0f || sampleRate_ <= 0.0) ? 1.0f : float(1.0 / (sampleRate_ * seconds));
    }

    Stage stage_ = Stage::Idle;
    double sampleRate_ = 0.0;
    float attackS_ = 0.01f, decayS_ = 0.1f, sustain_ = 0.8f, releaseS_ = 0.2f;
    float level_ = 0.0f;
    float releaseStart_ = 0.0f;
};

} // namespace soundx::engine
```

- [ ] **Step 5: Run tests to verify they pass**

```powershell
cmake --build build --config Release --target EngineTests
ctest --test-dir build -C Release --output-on-failure
```

Expected: all tests PASS.

- [ ] **Step 6: Commit**

```powershell
git add plugin/engine/Adsr.h tests/engine/AdsrTests.cpp tests/CMakeLists.txt
git commit -m "feat: linear ADSR envelope"
```

---

### Task 6: `soundx::engine::WavetableVoice`

**Files:**
- Create: `plugin/engine/WavetableVoice.h`
- Create: `tests/engine/VoiceTests.cpp`
- Modify: `tests/CMakeLists.txt` (add source)

- [ ] **Step 1: Add `engine/VoiceTests.cpp` to `EngineTests` sources in `tests/CMakeLists.txt`**

```cmake
add_executable(EngineTests
    engine/WavetableTests.cpp
    engine/OscillatorTests.cpp
    engine/AdsrTests.cpp
    engine/VoiceTests.cpp)
```

- [ ] **Step 2: Write the failing tests in `tests/engine/VoiceTests.cpp`**

```cpp
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
```

- [ ] **Step 3: Run tests to verify they fail**

```powershell
cmake --build build --config Release --target EngineTests
```

Expected: compile FAILURE — `engine/WavetableVoice.h: No such file`.

- [ ] **Step 4: Write `plugin/engine/WavetableVoice.h`**

```cpp
#pragma once
// JUCE-free. render() is RT-safe: writes additively into caller's buffer.
#include <cmath>
#include "Adsr.h"
#include "Wavetable.h"
#include "WavetableOscillator.h"

namespace soundx::engine {

class WavetableVoice {
public:
    explicit WavetableVoice(const Wavetable& wavetable) : osc_(wavetable) {}

    void setSampleRate(double sampleRate) {
        osc_.setSampleRate(sampleRate);
        env_.setSampleRate(sampleRate);
    }

    void setParams(float attackS, float decayS, float sustain01, float releaseS, float position01) {
        env_.setParams(attackS, decayS, sustain01, releaseS);
        osc_.setPosition(position01);
    }

    void noteOn(int midiNote, float velocity01) {
        velocity_ = velocity01;
        osc_.setFrequency(midiNoteToHz(midiNote));
        osc_.reset();
        env_.noteOn();
    }

    void noteOff() { env_.noteOff(); }
    void kill() { env_.reset(); }
    bool isActive() const { return env_.isActive(); }

    void render(float* dest, int numSamples) {
        if (!isActive())
            return;
        for (int i = 0; i < numSamples; ++i)
            dest[i] += osc_.nextSample() * env_.nextLevel() * velocity_;
    }

    static float midiNoteToHz(int midiNote) {
        return 440.0f * std::pow(2.0f, float(midiNote - 69) / 12.0f);
    }

private:
    WavetableOscillator osc_;
    Adsr env_;
    float velocity_ = 0.0f;
};

} // namespace soundx::engine
```

- [ ] **Step 5: Run tests to verify they pass**

```powershell
cmake --build build --config Release --target EngineTests
ctest --test-dir build -C Release --output-on-failure
```

Expected: all tests PASS.

- [ ] **Step 6: Commit**

```powershell
git add plugin/engine/WavetableVoice.h tests/engine/VoiceTests.cpp tests/CMakeLists.txt
git commit -m "feat: wavetable voice combining oscillator, envelope, velocity"
```

---

### Task 7: Wire the engine into the plugin (APVTS params + Synthesiser + render test)

**Files:**
- Create: `plugin/SynthVoice.h`
- Modify: `plugin/PluginProcessor.h`
- Modify: `plugin/PluginProcessor.cpp`
- Create: `tests/plugin/RenderTests.cpp`
- Modify: `tests/CMakeLists.txt` (add `PluginRenderTests` target)

- [ ] **Step 1: Add the `PluginRenderTests` target to `tests/CMakeLists.txt`** (append at the end)

```cmake
add_executable(PluginRenderTests
    plugin/RenderTests.cpp)
target_link_libraries(PluginRenderTests PRIVATE SoundX Catch2::Catch2WithMain)
catch_discover_tests(PluginRenderTests)
```

- [ ] **Step 2: Write the failing test in `tests/plugin/RenderTests.cpp`**

```cpp
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include "PluginProcessor.h"

namespace {
struct BlockResult {
    float peakRms = 0.0f;
    bool allFinite = true;
};

BlockResult renderBlocks(SoundXAudioProcessor& proc, juce::MidiBuffer& midi, int numBlocks, int blockSize) {
    BlockResult r;
    juce::AudioBuffer<float> buffer(2, blockSize);
    for (int b = 0; b < numBlocks; ++b) {
        buffer.clear();
        proc.processBlock(buffer, midi);
        midi.clear();
        r.peakRms = std::max(r.peakRms, buffer.getRMSLevel(0, 0, blockSize));
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            for (int i = 0; i < blockSize; ++i)
                if (!std::isfinite(buffer.getSample(ch, i)))
                    r.allFinite = false;
    }
    return r;
}
} // namespace

TEST_CASE("held note produces finite, audible output; release returns to silence") {
    juce::ScopedJuceInitialiser_GUI juceInit;
    SoundXAudioProcessor proc;
    proc.prepareToPlay(44100.0, 512);

    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.8f), 0);
    auto held = renderBlocks(proc, midi, 40, 512); // ~460ms
    REQUIRE(held.allFinite);
    REQUIRE(held.peakRms > 0.01f);

    midi.addEvent(juce::MidiMessage::noteOff(1, 60), 0);
    renderBlocks(proc, midi, 80, 512); // ~930ms >> max release tail at defaults
    juce::MidiBuffer empty;
    auto after = renderBlocks(proc, empty, 4, 512);
    REQUIRE(after.peakRms < 1.0e-4f);
}

TEST_CASE("state save and restore round-trips a parameter") {
    juce::ScopedJuceInitialiser_GUI juceInit;
    SoundXAudioProcessor proc;
    auto* gain = proc.apvts().getParameter("gain");
    REQUIRE(gain != nullptr);
    gain->setValueNotifyingHost(0.25f);

    juce::MemoryBlock state;
    proc.getStateInformation(state);

    gain->setValueNotifyingHost(0.9f);
    proc.setStateInformation(state.getData(), int(state.getSize()));
    REQUIRE(std::abs(gain->getValue() - 0.25f) < 1.0e-4f);
}
```

- [ ] **Step 3: Run to verify failure**

```powershell
cmake --build build --config Release --target PluginRenderTests
```

Expected: compile FAILURE — `SoundXAudioProcessor` has no member `apvts`; and at runtime the first test would fail with silence (processBlock only clears). Proceed to implement.

- [ ] **Step 4: Write `plugin/SynthVoice.h`** (JUCE bridge)

```cpp
#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <vector>
#include "engine/WavetableVoice.h"

class SynthSound : public juce::SynthesiserSound {
public:
    bool appliesToNote(int) override { return true; }
    bool appliesToChannel(int) override { return true; }
};

class SynthVoice : public juce::SynthesiserVoice {
public:
    explicit SynthVoice(const soundx::engine::Wavetable& wavetable) : voice_(wavetable) {}

    // Pre-allocates the scratch buffer. Call from prepareToPlay, never from audio thread.
    void prepare(double sampleRate, int maxBlockSize) {
        voice_.setSampleRate(sampleRate);
        scratch_.assign(size_t(maxBlockSize), 0.0f);
    }

    void setParams(float a, float d, float s, float r, float position) {
        voice_.setParams(a, d, s, r, position);
    }

    bool canPlaySound(juce::SynthesiserSound* sound) override {
        return dynamic_cast<SynthSound*>(sound) != nullptr;
    }

    void startNote(int midiNote, float velocity, juce::SynthesiserSound*, int) override {
        voice_.noteOn(midiNote, velocity);
    }

    void stopNote(float, bool allowTailOff) override {
        if (allowTailOff) {
            voice_.noteOff();
        } else {
            voice_.kill();
            clearCurrentNote();
        }
    }

    void pitchWheelMoved(int) override {}
    void controllerMoved(int, int) override {}

    void renderNextBlock(juce::AudioBuffer<float>& output, int startSample, int numSamples) override {
        if (!voice_.isActive()) {
            clearCurrentNote();
            return;
        }
        std::fill_n(scratch_.data(), size_t(numSamples), 0.0f);
        voice_.render(scratch_.data(), numSamples);
        for (int ch = 0; ch < output.getNumChannels(); ++ch)
            output.addFrom(ch, startSample, scratch_.data(), numSamples);
        if (!voice_.isActive())
            clearCurrentNote();
    }

private:
    soundx::engine::WavetableVoice voice_;
    std::vector<float> scratch_;
};
```

- [ ] **Step 5: Update `plugin/PluginProcessor.h`** (full replacement)

```cpp
#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "engine/Wavetable.h"

class SoundXAudioProcessor : public juce::AudioProcessor {
public:
    static constexpr int kNumVoices = 16;

    SoundXAudioProcessor();

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "SoundX"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& apvts() { return apvts_; }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    soundx::engine::Wavetable wavetable_ = soundx::engine::Wavetable::makeSineSaw();
    juce::Synthesiser synth_;
    juce::AudioProcessorValueTreeState apvts_;

    std::atomic<float>* gain_ = nullptr;
    std::atomic<float>* attack_ = nullptr;
    std::atomic<float>* decay_ = nullptr;
    std::atomic<float>* sustain_ = nullptr;
    std::atomic<float>* release_ = nullptr;
    std::atomic<float>* position_ = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SoundXAudioProcessor)
};
```

- [ ] **Step 6: Update `plugin/PluginProcessor.cpp`** (full replacement)

```cpp
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "SynthVoice.h"

namespace {
juce::NormalisableRange<float> secondsRange() {
    juce::NormalisableRange<float> r(0.001f, 5.0f);
    r.setSkewForCentre(0.3f);
    return r;
}
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout SoundXAudioProcessor::createParameterLayout() {
    using P = juce::AudioParameterFloat;
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add(std::make_unique<P>(juce::ParameterID{"gain", 1}, "Gain",
                                   juce::NormalisableRange<float>(0.0f, 1.0f), 0.8f));
    layout.add(std::make_unique<P>(juce::ParameterID{"attack", 1}, "Attack", secondsRange(), 0.01f));
    layout.add(std::make_unique<P>(juce::ParameterID{"decay", 1}, "Decay", secondsRange(), 0.1f));
    layout.add(std::make_unique<P>(juce::ParameterID{"sustain", 1}, "Sustain",
                                   juce::NormalisableRange<float>(0.0f, 1.0f), 0.8f));
    layout.add(std::make_unique<P>(juce::ParameterID{"release", 1}, "Release", secondsRange(), 0.2f));
    layout.add(std::make_unique<P>(juce::ParameterID{"position", 1}, "Position",
                                   juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));
    return layout;
}

SoundXAudioProcessor::SoundXAudioProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts_(*this, nullptr, "PARAMS", createParameterLayout()) {
    gain_ = apvts_.getRawParameterValue("gain");
    attack_ = apvts_.getRawParameterValue("attack");
    decay_ = apvts_.getRawParameterValue("decay");
    sustain_ = apvts_.getRawParameterValue("sustain");
    release_ = apvts_.getRawParameterValue("release");
    position_ = apvts_.getRawParameterValue("position");

    synth_.addSound(new SynthSound());
    for (int i = 0; i < kNumVoices; ++i)
        synth_.addVoice(new SynthVoice(wavetable_));
}

void SoundXAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    synth_.setCurrentPlaybackSampleRate(sampleRate);
    for (int i = 0; i < synth_.getNumVoices(); ++i)
        if (auto* v = dynamic_cast<SynthVoice*>(synth_.getVoice(i)))
            v->prepare(sampleRate, samplesPerBlock);
}

void SoundXAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    const float a = attack_->load(), d = decay_->load(), s = sustain_->load(),
                r = release_->load(), pos = position_->load();
    for (int i = 0; i < synth_.getNumVoices(); ++i)
        if (auto* v = dynamic_cast<SynthVoice*>(synth_.getVoice(i)))
            v->setParams(a, d, s, r, pos);

    synth_.renderNextBlock(buffer, midi, 0, buffer.getNumSamples());
    buffer.applyGain(gain_->load());
}

bool SoundXAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        || layouts.getMainOutputChannelSet() == juce::AudioChannelSet::mono();
}

juce::AudioProcessorEditor* SoundXAudioProcessor::createEditor() {
    return new SoundXAudioProcessorEditor(*this);
}

void SoundXAudioProcessor::getStateInformation(juce::MemoryBlock& destData) {
    if (auto xml = apvts_.copyState().createXml())
        copyXmlToBinary(*xml, destData);
}

void SoundXAudioProcessor::setStateInformation(const void* data, int sizeInBytes) {
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        if (xml->hasTagName(apvts_.state.getType()))
            apvts_.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new SoundXAudioProcessor();
}
```

- [ ] **Step 7: Build and run all tests**

```powershell
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Expected: all `EngineTests` and `PluginRenderTests` PASS.

- [ ] **Step 8: Commit**

```powershell
git add plugin/ tests/
git commit -m "feat: wire wavetable engine into plugin with APVTS params and render tests"
```

---

### Task 8: Minimal HUD-flavored editor with working controls

**Files:**
- Modify: `plugin/PluginEditor.h`
- Modify: `plugin/PluginEditor.cpp`

- [ ] **Step 1: Update `plugin/PluginEditor.h`** (full replacement)

```cpp
#pragma once
#include "PluginProcessor.h"

class SoundXAudioProcessorEditor : public juce::AudioProcessorEditor {
public:
    explicit SoundXAudioProcessorEditor(SoundXAudioProcessor&);
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    static constexpr int kNumSliders = 6;
    using Attachment = juce::AudioProcessorValueTreeState::SliderAttachment;

    struct LabeledSlider {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<Attachment> attachment;
    };

    SoundXAudioProcessor& processor_;
    std::array<LabeledSlider, kNumSliders> sliders_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SoundXAudioProcessorEditor)
};
```

- [ ] **Step 2: Update `plugin/PluginEditor.cpp`** (full replacement)

```cpp
#include "PluginEditor.h"

namespace {
constexpr auto kBackground = 0xff02090c;
constexpr auto kAccent = 0xff22d3ee;
constexpr auto kDim = 0xff0e3a40;

constexpr std::array<const char*, 6> kParamIds = {"gain", "attack", "decay",
                                                  "sustain", "release", "position"};
constexpr std::array<const char*, 6> kParamNames = {"GAIN", "ATK", "DEC",
                                                    "SUS", "REL", "POS"};
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
        addAndMakeVisible(s.slider);

        s.label.setText(kParamNames[size_t(i)], juce::dontSendNotification);
        s.label.setJustificationType(juce::Justification::centred);
        s.label.setColour(juce::Label::textColourId, juce::Colour(kAccent));
        s.label.setFont(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 12.0f, juce::Font::plain));
        addAndMakeVisible(s.label);

        s.attachment = std::make_unique<Attachment>(processor_.apvts(), kParamIds[size_t(i)], s.slider);
    }
    setResizable(true, true);
    setResizeLimits(540, 320, 1440, 850);
    setSize(720, 420);
}

void SoundXAudioProcessorEditor::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(kBackground));
    g.setColour(juce::Colour(kAccent));
    g.setFont(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 22.0f, juce::Font::plain));
    g.drawText("SOUNDX::ENGINE", getLocalBounds().removeFromTop(60), juce::Justification::centred);
    g.setColour(juce::Colour(kDim));
    g.drawRect(getLocalBounds().reduced(8), 1);
}

void SoundXAudioProcessorEditor::resized() {
    auto area = getLocalBounds().reduced(24);
    area.removeFromTop(60); // title
    const int cell = area.getWidth() / kNumSliders;
    for (auto& s : sliders_) {
        auto col = area.removeFromLeft(cell).reduced(8);
        s.label.setBounds(col.removeFromTop(18));
        s.slider.setBounds(col.withHeight(juce::jmin(col.getHeight(), 130)));
    }
}
```

- [ ] **Step 3: Build and verify manually in the standalone**

```powershell
cmake --build build --config Release
Start-Process "build\plugin\SoundX_artefacts\Release\Standalone\SoundX.exe"
```

Expected: dark HUD window, cyan title, six labeled rotary knobs. In the standalone's Options → Audio/MIDI settings, enable an input MIDI device or use the on-screen keyboard (Options → "Show MIDI keyboard" if available); playing notes produces a tone; POSITION morphs sine→saw brightness; ADSR knobs change the shape. Close it.

- [ ] **Step 4: Run full test suite to confirm nothing broke**

```powershell
ctest --test-dir build -C Release --output-on-failure
```

Expected: all PASS.

- [ ] **Step 5: Commit**

```powershell
git add plugin/PluginEditor.h plugin/PluginEditor.cpp
git commit -m "feat: HUD-styled editor with six parameter knobs"
```

---

### Task 9: CI — Windows build + tests + pluginval

**Files:**
- Create: `.github/workflows/build.yml`

- [ ] **Step 1: Write `.github/workflows/build.yml`**

```yaml
name: build

on:
  push:
    branches: ["**"]
  pull_request:

jobs:
  windows:
    runs-on: windows-latest
    steps:
      - uses: actions/checkout@v4

      - name: Configure
        run: cmake -B build -G "Visual Studio 17 2022" -A x64

      - name: Build
        run: cmake --build build --config Release

      - name: Run tests
        run: ctest --test-dir build -C Release --output-on-failure

      - name: Download pluginval
        run: |
          Invoke-WebRequest "https://github.com/Tracktion/pluginval/releases/latest/download/pluginval_Windows.zip" -OutFile pluginval.zip
          Expand-Archive pluginval.zip -DestinationPath pluginval

      - name: Validate plugin (pluginval, max strictness)
        run: pluginval\pluginval.exe --strictness-level 10 --skip-gui-tests --validate "build\plugin\SoundX_artefacts\Release\VST3\SoundX.vst3"

      - name: Upload VST3 artifact
        uses: actions/upload-artifact@v4
        with:
          name: SoundX-VST3-win64
          path: build/plugin/SoundX_artefacts/Release/VST3/
```

- [ ] **Step 2: Run pluginval locally first so CI isn't the first time** (download once)

```powershell
Invoke-WebRequest "https://github.com/Tracktion/pluginval/releases/latest/download/pluginval_Windows.zip" -OutFile "$env:TEMP\pluginval.zip"
Expand-Archive "$env:TEMP\pluginval.zip" -DestinationPath "$env:TEMP\pluginval" -Force
& "$env:TEMP\pluginval\pluginval.exe" --strictness-level 10 --skip-gui-tests --validate "build\plugin\SoundX_artefacts\Release\VST3\SoundX.vst3"
```

Expected: ends with `ALL TESTS PASSED`. If any test fails, fix before committing (common causes: state recall asymmetry, buffer-size handling — both already covered by our implementation).

- [ ] **Step 3: Commit**

```powershell
git add .github/
git commit -m "ci: Windows build, unit tests, pluginval at max strictness"
```

---

### Task 10: Install into FL Studio + docs + merge

**Files:**
- Modify: `README.md`

- [ ] **Step 1: Install the VST3 for FL Studio** (needs an elevated PowerShell because Common Files is admin-only — this is the standard VST3 location FL scans)

```powershell
# In an *elevated* PowerShell:
Copy-Item -Recurse -Force "build\plugin\SoundX_artefacts\Release\VST3\SoundX.vst3" "$env:CommonProgramFiles\VST3\"
```

Alternative without admin: in FL Studio go to Options → Manage plugins → add `<repo>\build\plugin\SoundX_artefacts\Release\VST3` as an extra search folder.

- [ ] **Step 2: Manual smoke test in FL Studio**

1. Open FL Studio → Options → Manage plugins → Find more plugins (scan).
2. Add "SoundX" to a channel.
3. Play notes; turn POSITION while holding a chord; automate GAIN.
4. Save the project, close FL, reopen — knob positions must be recalled.

Expected: sound plays, no crackles at default buffer size, state recalls.

- [ ] **Step 3: Update `README.md`** — replace the "Development" section at the end with:

```markdown
## Building the plugin

```powershell
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Artifacts land in `build/plugin/SoundX_artefacts/Release/` (`VST3/` and `Standalone/`).
Install for FL Studio by copying `SoundX.vst3` to `C:\Program Files\Common Files\VST3\`
(elevated shell) or adding the artefacts folder as an FL plugin search path.

## Development

- C++ tests: `ctest --test-dir build -C Release --output-on-failure`
- Python factory tests: `uv run pytest`
- Lint (Python): `uv run ruff check .`
```

- [ ] **Step 4: Commit and merge to main**

```powershell
git add README.md
git commit -m "docs: build and FL Studio install instructions"
git checkout main
git merge --no-ff feat/plan1-skeleton-wavetable -m "merge: plan 1 - plugin skeleton + wavetable engine"
```

---

## Self-review notes

- **Spec coverage (plan-1 scope):** JUCE-free engine ✓, RT rules (no alloc/locks in render; scratch pre-allocated in `prepare`) ✓, state recall ✓, pluginval max strictness in CI ✓, offline render test (non-silence + finite samples) ✓, HUD color language ✓. Deferred to later plans per roadmap: granular/spectral, morph, mod matrix, FX, OpenGL visualizer, presets/factory.
- **Type consistency check:** `Wavetable::sample(phase, position)`, `WavetableOscillator(const Wavetable&)`, `Adsr::setParams(a,d,s,r)`, `WavetableVoice::setParams(a,d,s,r,position)`, `SynthVoice::prepare(sampleRate, maxBlockSize)`, `SoundXAudioProcessor::apvts()` — used consistently across tasks.
- **Known risk:** JUCE tag `8.0.4` / Catch2 `v3.7.1` availability — fallback instruction included in Task 1/2. `pluginval` strictness 10 can surface real bugs; that is the point — fix, don't lower.
