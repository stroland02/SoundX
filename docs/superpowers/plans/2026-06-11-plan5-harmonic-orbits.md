# SoundX Plan 5: Harmonic Orbits Visualizer + Consonance Analysis

> Executed inline by the authoring session.

**Goal:** The spec's centerpiece visual: a real-time 3D orbital system where the output's partials orbit the fundamental — orbit radius from frequency ratio, body size from amplitude, color from inharmonicity — with a consonance ("stability") readout, interval naming, and a 4D-style projection twist driven by the MORPH parameter.

**Conscious scope decision:** v1 renders with software-projected 3D via juce::Graphics at 30 fps (this is the spec's own degradation-ladder fallback, promoted to default). A GL-shader version is a later upgrade; the visual design, math, and data pipeline are identical. Audio is never affected by the visualizer (lock-free FIFO tap, UI-side FFT).

## Components
1. `plugin/engine/analysis/Consonance.h` (JUCE-free, TDD):
   - Plomp–Levelt pairwise roughness over a partial set
   - `consonanceScore01()` — spec test: a perfect fifth (6-harmonic complexes) must score higher than a tritone
   - `nearestInterval(ratio)` — tempered interval name + cents deviation (P5 at 1.5 ⇒ +2¢)
2. Processor tap: `juce::AbstractFifo` mono copy of the post-FX output, pushed RT-safe each block; editor pulls on a timer.
3. `plugin/OrbitView.h/.cpp`: juce Component + Timer (30 Hz) — accumulates 2048 samples, Hann+FFT (reuses engine `fft`), peak-picks top 16 partials, animates orbits (yaw auto-rotation; roll = morph×90° for the 4D twist), draws rings/bodies/trails/core-glow, HUD text: fundamental Hz, strongest interval + cents, STABILITY %.
4. Editor: orbit canvas between the morph row and shared knobs.

## Tests
- Consonance: P5 > TT ordering, score ∈ (0,1], interval naming (1.5⇒P5, 2.0⇒Octave, cents accuracy ±3).
- Build + 65-test suite green + pluginval 10.
