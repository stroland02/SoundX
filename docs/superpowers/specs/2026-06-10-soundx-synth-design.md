# SoundX — Hybrid Morphing Synthesizer: Design Spec

**Date:** 2026-06-10
**Status:** Approved by owner (brainstorming session)
**Target:** VST3, Windows-first (FL Studio primary host), free release under GPLv3

## Vision

A free hybrid synthesizer whose identity is: *create beautiful, unique sounds with ease, guided by visuals that teach the music theory of what you're hearing.* Two hooks no current synth combines:

1. **The Morph Field** — two engine slots morphed not by crossfade but by true parameter + spectral interpolation, visualized as one rotating 4D object.
2. **Theory-accurate interactive visuals** — a 3D/4D Harmonic Orbits display where consonance, intervals, and spectral content are physically legible.

### Market rationale (research, June 2026)

- Hybrid synthesis (wavetable + granular + spectral) dominates current releases; no leader owns "seamless tri-engine morphing."
- UI is a known industry weak point; GPU-rendered, resizable, visualization-rich UIs are differentiators.
- Neural/AI features are the next wave but v1 ships without them (deliberate scope cut; the architecture leaves room).
- Free releases (Vital model) spread fastest and build the developer's name; GPLv3 + JUCE makes this licensing-clean.

## Product definition — v1 scope

### Sound engine
- **2 engine slots (A and B).** Each slot runs one of three engine modes:
  - **Wavetable** — classic table-scanning oscillator with position/warp controls.
  - **Granular** — grain cloud playback of a loaded sample (grain size, density, spray, pitch jitter).
  - **Spectral** — partial-based resynthesis of a loaded sample (stretch, shift, blur of spectral frames).
- **Morph Field** — XY pad. X axis = A↔B morph implemented as parameter-set interpolation plus spectral-frame interpolation (morph=0.0 is bit-exact engine A; morph=1.0 bit-exact engine B). Y axis = assignable macro (default: spectral brightness).
- **Universal sample import** — dragging any audio file in produces ALL THREE representations at once (single-cycle wavetable extraction, grain source, spectral model). User flips engine mode on the same sample with one click. Import runs on a background thread.
- **Modulation:** 3 LFOs, 2 envelopes (amp env + assignable), 4 macros. Drag-and-drop assignment onto any knob; modulated knobs show live motion rings.
- **FX rack (serial, reorderable):** distortion, chorus, delay, reverb, multiband compressor (OTT-style).
- **Polyphony:** 16 voices max, voice stealing, unison up to 8 with detune/spread.
- **Presets:** versioned JSON format; 50+ factory presets at launch, generated/curated via the Python factory pipeline.

### Out of scope for v1 (v1.1+ candidates)
AI text-to-patch, neural oscillator, macOS/AU build, MPE, preset cloud sharing, microtonal tuning files.

## UI design

**Direction:** "Holographic HUD" — sci-fi instrument cluster: dark background, wireframe 3D visualizers, monospace readouts, scan-line texture, single neon accent (cyan), secondary accent (magenta) for warnings/dissonance. Resizable, OpenGL-rendered.

### Centerpiece: Harmonic Orbits visualizer (3D/4D)

The defining feature. A real-time 3D orbital system:

- **Core** = fundamental of the playing note; **orbiting bodies** = partials.
- Orbit radius = frequency ratio to fundamental; body size = partial amplitude.
- **Harmonicity is legible physics:** harmonic partials ride stable glowing rings; detuned partials wobble visibly; inharmonic content (granular/spectral engines) traces eccentric comet paths.
- **Chords are constellations:** each note spawns a system; consonant intervals share interlocking orbital resonances (3:2 for a fifth), dissonant intervals produce visible interference shimmer where rings collide.
- **The 4D morph:** the patch is presented as one 4D object; Engine A and Engine B are two 3D slices of it. Dragging the Morph Field applies a 4D→3D projection rotation — B's partials emerge through the geometry as A's recede, mirroring the actual spectral interpolation. Tagline: *"Your sound is a 4D object. Turn it."*
- **Useful, not decorative:**
  - Hover any body → readout: harmonic number, interval name vs root, cents deviation, and the parameter that most affects it.
  - Scale-lock mode: pick key/scale; out-of-key pitch content tints red.
  - Consonance/stability score (psychoacoustic roughness model) shown as core ring-glow + numeric readout — one-glance tension feedback.
  - Mouse-grab rotation; slow auto-orbit when idle.
- **Degradation ladder:** full 3D/4D GL → flat 2D orbit mode (weak GPU) → static panel (GL context failure). Audio is never affected by visualizer state.

### Layout (main panel)
- Center: Harmonic Orbits canvas with HUD readouts.
- Below center: Morph Field XY pad.
- Left/right flanks: slot A / slot B engine panels (mode switch + per-mode controls).
- Bottom: mod sources (LFOs/envs/macros, drag handles) + FX rack tabs.
- Top bar: preset browser, scale-lock, settings, GPU/CPU meter (in-theme).

## Architecture

Two deliverables in one repo:

### 1. The plugin (C++20, JUCE 8, CMake, VST3)

```
plugin/
├── engine/            # real-time DSP core (JUCE-independent where practical)
│   ├── SoundSource.h          # common interface: note events + params -> audio buffer
│   ├── WavetableVoice.*
│   ├── GranularVoice.*
│   ├── SpectralVoice.*
│   ├── MorphController.*      # 4D interpolation: param sets + spectral frames
│   ├── SampleImporter.*       # bg-thread analysis -> all 3 representations
│   ├── ModMatrix.*
│   └── effects/               # distortion, chorus, delay, reverb, multiband comp
├── analysis/          # shared math: partial tracking, roughness/consonance, interval naming
├── ui/                # JUCE components + OpenGL orbit visualizer
└── plugin/            # processor/editor glue, parameter layout, state save/load
```

**Threading model:** audio thread publishes spectral snapshots (partial freq/amp arrays) into a lock-free FIFO; the GL renderer consumes at 60fps. UI never reads audio state directly; parameters flow through JUCE's APVTS.

### 2. The factory (Python 3.12 + CUDA venv, already scaffolded)

Not shipped to users. Generates and validates plugin content:
- Factory wavetable bank generation.
- Pre-rendered spectral models for factory presets.
- GPU brute-force search for interesting morph pairs (candidate preset discovery).
- Preset CI validation: every preset loads, renders non-silent audio, stays under CPU budget.

Outputs binary assets embedded into the plugin at build time.

### Build & CI
- CMake + JUCE 8 (fetched via CPM or submodule).
- GitHub Actions: Windows build on every push; unit tests; offline render tests; `pluginval` at max strictness.
- License: **GPLv3** (enables free JUCE use; Vital-proven model for free releases).

## Performance & error handling

### Real-time rules (enforced in code review)
- Audio thread: no allocation, no locks, no I/O, no JUCE UI calls. Pre-allocated buffers; lock-free FIFOs only.
- CPU budget: ≤ ~1% of one mid-range core per voice; 8-voice poly + FX ≤ ~15%. Spectral engine exposes a quality knob (partial count vs CPU).
- Visualizer is sacrificial — degrades before audio ever suffers.

### Error handling
- Sample import: unsupported/corrupt files rejected with HUD message; oversized files truncated for analysis with notice; never crash.
- Presets: versioned JSON; unknown fields ignored; missing fields defaulted — forward/backward compatible.
- Must survive: sample-rate/buffer-size changes mid-session, FL bridged/unbridged modes, multiple instances, full state recall on project reload.

## Testing

| Layer | Tool | What it proves |
|---|---|---|
| DSP unit tests | Catch2 | Wavetable interpolation; morph endpoints bit-exact; consonance scoring orders fifth > tritone; mod routing |
| Offline render tests | CI harness → WAV | Known patches render non-silent, no NaNs/clipping, expected spectral content |
| Host stability | `pluginval` (max strictness) in CI | DAW lifecycle: state recall, SR changes, multi-instance |
| Factory/presets | pytest | Every shipped preset loads, renders, meets CPU budget |
| Release gate | Manual FL Studio session | Real-world drag-in, automation, render |

## Decisions log

| Decision | Choice | Why |
|---|---|---|
| Plugin type | Synth/instrument | Owner choice; biggest wow potential |
| Core hook | Hybrid tri-engine + morph | Rides dominant market trend with a unique morphing twist |
| Distribution | Free (GPLv3) | Build a name; Vital-proven; fastest spread |
| UI direction | Holographic HUD | Owner choice from 4 mockups |
| Theory visual | Harmonic Orbits, 3D/4D | Owner choice; accurate physics-of-music, marketable 4D morph |
| Stack | JUCE/C++ + OpenGL | Industry standard; no ceilings; GPL-compatible |
| Python/CUDA env | Content factory + CI | Keeps GPU investment useful without shipping Python |
