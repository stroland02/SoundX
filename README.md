# SoundX

GPU-accelerated audio application.

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
