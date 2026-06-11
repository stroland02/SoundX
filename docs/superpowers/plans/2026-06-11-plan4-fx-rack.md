# SoundX Plan 4: FX Rack

> **For agentic workers:** REQUIRED SUB-SKILL: superpowers:subagent-driven-development or superpowers:executing-plans. (Executed inline by the authoring session.)

**Goal:** Five master effects after the synth: distortion → OTT-style multiband compressor → chorus → delay → reverb. Each toggleable with focused parameters, JUCE-free implementations under `plugin/engine/effects/`, covered by behavior tests.

**Deferred:** reorderable chain (spec mentions it; fixed order v1), per-effect advanced params, sidechain, FX on the HUD (Plan 5 restyles the whole UI).

## Effects & parameters
| FX | Params | Implementation |
|---|---|---|
| Distortion | `dist_on, dist_drive(1–20), dist_mix` | normalized tanh waveshaper: `tanh(x·drive)/tanh(drive)` |
| OTT comp | `comp_on, comp_depth(0–1)` | 3-band split (one-pole crossovers 200 Hz / 2 kHz, perfect-sum by construction), per-band up+down compression `gain=(ref/env)^(0.6·depth)` clamped ×0.25..×4 |
| Chorus | `chorus_on, chorus_rate(0.1–5Hz), chorus_depth(1–15ms), chorus_mix` | modulated delay line per channel, R channel LFO offset 90° for width |
| Delay | `delay_on, delay_time(10–2000ms), delay_feedback(0–0.95), delay_mix` | stereo circular buffer, interpolated read |
| Reverb | `reverb_on, reverb_size, reverb_damp, reverb_mix` | Freeverb topology: 8 combs + 4 allpasses per channel, +23-sample stereo spread, tunings scaled to sample rate |

All: `prepare(sampleRate, maxBlock)` allocates; `process(L, R, n)` is allocation-free; `mix=0`/`depth=0`/`off` is bit-exact passthrough.

## Tests (TDD)
- Distortion: bounded output, mix 0 passthrough exact, drive increases harmonic content (RMS of difference).
- Comp: loud/quiet RMS ratio compressed vs input ratio; depth 0 passthrough.
- Chorus: output differs from input when on; finite; mix 0 passthrough.
- Delay: impulse echoes at the delay time with feedback decay; mix 0 passthrough.
- Reverb: impulse grows a tail that decays over time; mix 0 passthrough; finite.
- Render test: each FX enabled alone keeps output finite/audible; all five at once finite.

## Tasks
1. `effects/` headers + `tests/engine/EffectsTests.cpp` (TDD).
2. Processor: params, prepare, post-synth chain in processBlock; render test.
3. Editor FX row (toggles + knobs).
4. pluginval 10, README, merge, push.
