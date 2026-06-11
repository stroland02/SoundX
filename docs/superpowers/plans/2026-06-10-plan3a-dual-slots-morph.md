# SoundX Plan 3a: Dual Slots + Morph Core

> **For agentic workers:** REQUIRED SUB-SKILL: superpowers:subagent-driven-development or superpowers:executing-plans. (Executed inline by the authoring session.)

**Goal:** Two engine slots (A and B), each with its own mode, parameters, and dropped sample, blended by a MORPH control whose endpoints are exact: morph=0 is purely slot A, morph=1 purely slot B. Spectral↔spectral morphs interpolate the partial data itself (frequencies glide), not the audio.

**Deferred to Plan 3b:** mod matrix (LFOs/envelopes/macros, drag-drop modulation), Morph Field XY pad (Y-axis macro), per-voice morph smoothing.

## Morph semantics (the load-bearing decisions)

| Slot A mode | Slot B mode | Blend strategy | Why |
|---|---|---|---|
| spectral | spectral | **Model interpolation** inside one voice: pairwise partial lerp (freq glides 440→660 through 550) | The "sounds neither engine makes alone" requirement |
| wavetable | wavetable | Linear crossfade of both voices | Phase-locked same-pitch oscillators ⇒ linear output blend ≡ table-content blend |
| anything else | anything else | Equal-power crossfade (cos/sin) | Uncorrelated sources; keeps loudness constant |

Both slots receive `noteOn` regardless of morph so mid-note morph automation always has a tail to reveal; rendering skips a slot whose gain < 1e-4 (endpoint CPU cost unchanged).

## Parameters (replaces the single-engine set; pre-release, no migration)

Shared: `gain, attack, decay, sustain, release, morph(0..1, default 0)`.
Per slot (`a_`/`b_` prefixes): `mode{Wavetable,Granular,Spectral}, position, grainsize, density, spray, stretch`.

## Engine changes
- `WavetableOscillator`: `setTables(const Wavetable* a, const Wavetable* b)` + `setMorph` — blended dual-bank read (B null ⇒ pure A). `WavetableVoice` passes through.
- `SpectralVoice`: `setModelB(const SpectralModel*)` + `setMorph` — frame lookup from both models (clamped to each model's length, timeline driven by model A), pairwise partial lerp with amp-guarded frequency (an empty partial adopts the other side's frequency instead of lerping from 0 Hz).
- `GranularVoice`: unchanged (two instances crossfade).

## Plugin changes
- `SynthVoice`: two `Slot` structs {wt, gran, spec, mode}; gain table per the matrix above; renders each audible slot into scratch and mixes.
- Processor: per-slot assets (sample, wavetable bank, spectral model), `applySample(slot, data, name)`, `loadSampleFile(slot, file)`; param push extended.
- Editor: MORPH slider center-stage, two slot panels (mode box + 5 knobs + drop zone each; drop left half = A, right half = B), shared row (GAIN + ADSR). 17 sliders total.

## Tasks (TDD)
1. Engine: dual-bank wavetable blend + spectral model morph — tests: blend endpoints/midpoint exact on constant banks; spectral morph zero-crossings ≈440/550/660 at morph 0/0.5/1.
2. Plugin wiring — render tests: morph=0 default A audible; morph=1 + B granular sample audible; morph=1 + B empty granular silent; spectral-spectral morph midpoint audible & finite.
3. Editor dual-slot layout.
4. pluginval 10, README, merge, push.
