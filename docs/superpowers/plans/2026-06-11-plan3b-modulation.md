# SoundX Plan 3b: Modulation System (LFOs + Macros)

> **For agentic workers:** REQUIRED SUB-SKILL: superpowers:subagent-driven-development or superpowers:executing-plans. (Executed inline by the authoring session.)

**Goal:** 3 LFOs (sine/triangle/saw/square/S&H, 0.01–20 Hz) and 4 macro knobs, each with a destination selector and bipolar amount, modulating any of the 12 engine parameters (morph, gain, and both slots' position/grainsize/density/spray/stretch) at control rate.

**Deferred to a later plan:** drag-and-drop assignment, live motion rings on modulated knobs, multi-destination routing matrix, assignable mod envelope, per-voice LFOs.

## Design decisions
- **One destination per source** (13-way choice param incl. "Off") — keeps every route a plain automatable parameter; APVTS handles persistence with zero custom state code. The full matrix comes later.
- **Control-rate modulation**: each LFO advances once per audio block; offsets are applied to the parameter values pushed to voices. Bipolar LFOs (−1..1), unipolar macros (0..1), amount −1..1, scaled by each destination's natural range and clamped to its bounds.
- LFOs are global (not per-voice) and free-running.

## New parameters
Per LFO i∈{1,2,3}: `lfo{i}_rate` (0.01–20 Hz, skewed), `lfo{i}_shape` (choice), `lfo{i}_dest` (choice), `lfo{i}_amount` (−1..1).
Per macro m∈{1..4}: `macro{m}` (0..1), `macro{m}_dest`, `macro{m}_amount`.

## Files
- `plugin/engine/Lfo.h` (NEW, JUCE-free, RT-safe advance) + `tests/engine/LfoTests.cpp`
- `plugin/PluginProcessor.h/.cpp` (params, mod offsets in processBlock)
- `plugin/PluginEditor.h/.cpp` (MOD row: 7 source columns)
- `tests/plugin/RenderTests.cpp` (deterministic macro→morph test: macro1 routed to morph at amount 1 flips output between audible slot A and silent empty slot B)

## Tasks (TDD)
1. `Lfo` — tests: sine quarter-phase = 1, triangle/saw shape points, square sign flip, period accuracy over advances, S&H bounded and changing across wraps.
2. Processor mod routing — render tests: macro routed to morph silences/un-silences deterministically; LFO routed with amount 0 changes nothing.
3. Editor MOD row.
4. pluginval 10, README, merge, push.
