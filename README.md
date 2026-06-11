# SoundX

Hybrid synthesizer VST3 (Windows). Current features:

- **Dual engine slots (A/B)** — each slot has its own sample, engine mode, and
  parameters; the MORPH control blends them. Spectral↔spectral morphs
  interpolate the partials themselves, so frequencies glide through sounds
  neither slot makes alone
- 16-voice wavetable synth (sine→saw factory bank, position morphing)
- **Granular engine** — drag any audio file in and play it as a grain cloud
  (grain size / density / spray), pitch-tracked across the keyboard
- **Spectral engine** — the same sample resynthesized from 32 tracked partials;
  STRETCH time-stretches independently of pitch (0 = spectral freeze)
- **Universal sample import** — one dropped sample becomes all three engines
  (wavetable bank, grain cloud, spectral model); switch with the mode selector
- HUD-styled resizable UI, full parameter automation and state recall

## Requirements

- Windows with an NVIDIA GPU (developed against an RTX 2080, driver 596+)
- Python 3.12+
- [uv](https://docs.astral.sh/uv/) for environment management

## Setup

```powershell
uv sync
```

This creates `.venv/` and installs the CUDA-enabled PyTorch stack
(torch + torchaudio from the cu128 index) plus numpy and soundfile.

Verify the GPU is usable:

```powershell
uv run python scripts/check_gpu.py
```

## Project tooling

Claude Code agent tooling (rules, skills, commands, hooks) from
[ECC](https://github.com/affaan-m/ECC) is installed per-project under
`.claude/` (developer profile + machine-learning module).

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
