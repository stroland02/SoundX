# SoundX Plan 2b: Spectral Engine

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** The third engine: a dropped sample is analyzed into time-varying spectral partials (STFT peak tracking) and resynthesized by a bank of sine oscillators — pitch-shiftable and time-stretchable independently, completing the spec's "one sample becomes all three engines."

**Architecture:** JUCE-free `Fft` (radix-2, offline) + `SpectralAnalyzer` (Hann STFT → top-32 partials per frame, parabolic frequency refinement) produce a `SpectralModel`. `SpectralVoice` (a `SoundSource`) renders it with 32 phase-accumulating sine oscillators, interpolating between frames; STRETCH scales playback speed (0 = spectral freeze). Plugin gains a third mode and a stretch parameter; analysis runs in the existing background import path.

**Tech Stack:** unchanged. **Prereq:** Plan 2a merged (37 tests green).

## File structure

```
plugin/engine/Fft.h               # NEW: offline radix-2 complex FFT
plugin/engine/SpectralModel.h     # NEW: partial frames + analyzer
plugin/engine/SpectralVoice.h     # NEW: additive resynthesis voice
plugin/PluginProcessor.h/.cpp     # MOD: 3-way mode, stretch param, model storage
plugin/SynthVoice.h               # MOD: third voice + dispatch
plugin/PluginEditor.cpp/.h        # MOD: SPECTRAL mode item, STRETCH knob (10 sliders)
tests/engine/SpectralTests.cpp    # NEW: analyzer + voice tests
tests/plugin/RenderTests.cpp      # MOD: spectral mode render test
```

### Key decisions
- Partials per frame: 32, sorted by frequency ascending (smooth frame-to-frame interpolation).
- Frame grid: window 2048, hop 512 → ~86 frames/sec at 44.1k.
- Note 60 plays original pitch (matches granular). Playback past the last frame holds it (sustained pads) while the envelope is active.
- Analysis cost ~100 ms/30 s sample, done inside the existing `applySample` (already off the audio thread for file drops; acceptable on the message thread v1).
- Amp calibration: Hann window coherent gain 0.5, single-sided spectrum → sine of amplitude A shows a peak magnitude ≈ A·N/4; analyzer scales by 4/N.

### Task list (TDD per task, code in each step)
1. `Fft.h` + `SpectralModel.h`/analyzer — tests: pure sine → one dominant partial at 440 Hz, amp ≈ 1; two-tone → both partials; silence → empty/quiet frames.
2. `SpectralVoice.h` — tests: synthetic single-partial model renders a 440 Hz tone (zero-crossing count), pitch tracks (+1 octave → ~2× crossings), no model → silent+safe, release decays to silence, stretch 0 keeps sounding (freeze).
3. Plugin wiring — `mode` becomes {Wavetable, Granular, Spectral}; new `stretch` param (0–4, default 1, skewed); `applySample` also builds the SpectralModel; SynthVoice owns the third voice. Render test: spectral mode + applySample → audible, finite.
4. Editor — third mode item, STRETCH knob (kNumSliders = 10).
5. pluginval strictness 10, README, merge.

(Complete code lives in the implementation commits; this plan was executed inline by the controller session that wrote it, with code authored against the structures above. The authoritative reference for types is the code itself plus the tests written first in each task.)
