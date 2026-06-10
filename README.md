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

## Development

- Tests: `uv run pytest`
- Lint: `uv run ruff check .`
