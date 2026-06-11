"""Factory preset generator for SoundX.

Writes presets/factory/*.soundxpreset (APVTS state XML). Every preset emits
the FULL parameter set (defaults overlaid with per-preset overrides) so loads
are deterministic regardless of the plugin's current state.

Run from the repo root:  uv run python factory/generate_presets.py
"""

from __future__ import annotations

import xml.etree.ElementTree as ET
from pathlib import Path

OUTPUT_DIR = Path(__file__).resolve().parent.parent / "presets" / "factory"

# Full parameter set with plugin defaults (kept in sync with
# PluginProcessor::createParameterLayout — validated by pytest ranges below).
DEFAULTS: dict[str, float] = {
    "gain": 0.8, "attack": 0.01, "decay": 0.1, "sustain": 0.8, "release": 0.2,
    "morph": 0.0,
    "a_mode": 0, "a_position": 0.0, "a_grainsize": 100.0, "a_density": 30.0,
    "a_spray": 0.2, "a_stretch": 1.0,
    "b_mode": 0, "b_position": 0.0, "b_grainsize": 100.0, "b_density": 30.0,
    "b_spray": 0.2, "b_stretch": 1.0,
    "lfo1_rate": 1.0, "lfo1_shape": 0, "lfo1_dest": 0, "lfo1_amount": 0.0,
    "lfo2_rate": 1.0, "lfo2_shape": 0, "lfo2_dest": 0, "lfo2_amount": 0.0,
    "lfo3_rate": 1.0, "lfo3_shape": 0, "lfo3_dest": 0, "lfo3_amount": 0.0,
    "macro1": 0.0, "macro1_dest": 0, "macro1_amount": 0.0,
    "macro2": 0.0, "macro2_dest": 0, "macro2_amount": 0.0,
    "macro3": 0.0, "macro3_dest": 0, "macro3_amount": 0.0,
    "macro4": 0.0, "macro4_dest": 0, "macro4_amount": 0.0,
    "dist_on": 0, "dist_drive": 4.0, "dist_mix": 1.0,
    "comp_on": 0, "comp_depth": 0.5,
    "chorus_on": 0, "chorus_rate": 1.0, "chorus_depth": 8.0, "chorus_mix": 0.5,
    "delay_on": 0, "delay_time": 350.0, "delay_feedback": 0.35, "delay_mix": 0.3,
    "reverb_on": 0, "reverb_size": 0.5, "reverb_damp": 0.5, "reverb_mix": 0.3,
}

# Parameter ranges for validation (min, max).
RANGES: dict[str, tuple[float, float]] = {
    "gain": (0, 1), "attack": (0.001, 5), "decay": (0.001, 5), "sustain": (0, 1),
    "release": (0.001, 5), "morph": (0, 1),
    "a_mode": (0, 2), "a_position": (0, 1), "a_grainsize": (5, 500),
    "a_density": (0.5, 100), "a_spray": (0, 1), "a_stretch": (0, 4),
    "b_mode": (0, 2), "b_position": (0, 1), "b_grainsize": (5, 500),
    "b_density": (0.5, 100), "b_spray": (0, 1), "b_stretch": (0, 4),
    **{f"lfo{i}_rate": (0.01, 20) for i in (1, 2, 3)},
    **{f"lfo{i}_shape": (0, 4) for i in (1, 2, 3)},
    **{f"lfo{i}_dest": (0, 12) for i in (1, 2, 3)},
    **{f"lfo{i}_amount": (-1, 1) for i in (1, 2, 3)},
    **{f"macro{m}": (0, 1) for m in (1, 2, 3, 4)},
    **{f"macro{m}_dest": (0, 12) for m in (1, 2, 3, 4)},
    **{f"macro{m}_amount": (-1, 1) for m in (1, 2, 3, 4)},
    "dist_on": (0, 1), "dist_drive": (1, 20), "dist_mix": (0, 1),
    "comp_on": (0, 1), "comp_depth": (0, 1),
    "chorus_on": (0, 1), "chorus_rate": (0.1, 5), "chorus_depth": (1, 15),
    "chorus_mix": (0, 1),
    "delay_on": (0, 1), "delay_time": (10, 2000), "delay_feedback": (0, 0.95),
    "delay_mix": (0, 1),
    "reverb_on": (0, 1), "reverb_size": (0, 1), "reverb_damp": (0, 1),
    "reverb_mix": (0, 1),
}

PRESETS: dict[str, dict[str, float]] = {
    "00 Init": {},
    "01 Glass Pad": {
        "attack": 0.9, "release": 1.6, "sustain": 0.7, "a_position": 0.25,
        "b_position": 0.55, "morph": 0.35, "chorus_on": 1, "chorus_mix": 0.45,
        "reverb_on": 1, "reverb_size": 0.75, "reverb_mix": 0.45,
        "lfo1_rate": 0.08, "lfo1_dest": 1, "lfo1_amount": 0.35,
    },
    "02 Neon Pluck": {
        "attack": 0.001, "decay": 0.18, "sustain": 0.0, "release": 0.25,
        "a_position": 0.85, "delay_on": 1, "delay_time": 310.0,
        "delay_feedback": 0.45, "delay_mix": 0.35,
    },
    "03 Saw Stack": {
        "a_position": 1.0, "b_position": 0.9, "morph": 0.5, "comp_on": 1,
        "comp_depth": 0.6, "chorus_on": 1, "chorus_depth": 12.0,
    },
    "04 Morph Drift": {
        "a_position": 0.1, "b_position": 0.95, "lfo1_rate": 0.05,
        "lfo1_dest": 1, "lfo1_amount": 1.0, "morph": 0.5, "reverb_on": 1,
        "reverb_mix": 0.35,
    },
    "05 Deep Bass": {
        "a_position": 0.6, "attack": 0.002, "decay": 0.3, "sustain": 0.85,
        "release": 0.12, "dist_on": 1, "dist_drive": 6.0, "dist_mix": 0.5,
        "comp_on": 1, "comp_depth": 0.7,
    },
    "06 Shimmer Keys": {
        "attack": 0.004, "decay": 0.6, "sustain": 0.4, "release": 0.8,
        "a_position": 0.35, "chorus_on": 1, "chorus_rate": 0.6,
        "chorus_mix": 0.6, "reverb_on": 1, "reverb_size": 0.85,
        "reverb_damp": 0.3, "reverb_mix": 0.5,
    },
    "07 Wobble Bass": {
        "a_position": 1.0, "sustain": 1.0, "release": 0.1,
        "lfo1_rate": 4.0, "lfo1_shape": 0, "lfo1_dest": 3, "lfo1_amount": 0.9,
        "dist_on": 1, "dist_drive": 9.0, "dist_mix": 0.65, "comp_on": 1,
        "comp_depth": 0.8,
    },
    "08 Slow Bloom": {
        "attack": 2.4, "release": 2.8, "a_position": 0.15, "b_position": 0.7,
        "morph": 0.0, "lfo2_rate": 0.04, "lfo2_dest": 1, "lfo2_amount": 0.9,
        "reverb_on": 1, "reverb_size": 0.95, "reverb_mix": 0.55,
    },
    "09 Radio Harsh": {
        "a_position": 0.95, "dist_on": 1, "dist_drive": 18.0, "dist_mix": 0.9,
        "comp_on": 1, "comp_depth": 1.0, "delay_on": 1, "delay_time": 95.0,
        "delay_feedback": 0.25, "delay_mix": 0.2,
    },
    "10 Grain Cloud (drop sample)": {
        "a_mode": 1, "a_grainsize": 180.0, "a_density": 18.0, "a_spray": 0.55,
        "a_position": 0.4, "attack": 0.4, "release": 1.2, "reverb_on": 1,
        "reverb_mix": 0.4,
    },
    "11 Spectra Freeze (drop sample)": {
        "a_mode": 2, "a_stretch": 0.0, "a_position": 0.5, "attack": 0.8,
        "release": 2.0, "reverb_on": 1, "reverb_size": 0.9, "reverb_mix": 0.5,
    },
    "12 Tape Chorus": {
        "a_position": 0.45, "chorus_on": 1, "chorus_rate": 0.25,
        "chorus_depth": 14.0, "chorus_mix": 0.7, "lfo3_rate": 0.11,
        "lfo3_dest": 3, "lfo3_amount": 0.15,
    },
    "13 Cathedral": {
        "attack": 1.2, "release": 3.5, "a_position": 0.2, "reverb_on": 1,
        "reverb_size": 1.0, "reverb_damp": 0.15, "reverb_mix": 0.65,
        "delay_on": 1, "delay_time": 620.0, "delay_feedback": 0.55,
        "delay_mix": 0.3,
    },
}


def preset_xml(overrides: dict[str, float]) -> str:
    params = DEFAULTS | overrides
    root = ET.Element("PARAMS")
    for pid, value in params.items():
        ET.SubElement(root, "PARAM", id=pid, value=f"{float(value):g}")
    ET.indent(root)
    return ET.tostring(root, encoding="unicode", xml_declaration=True)


def safe_filename(name: str) -> str:
    keep = "".join(c if c.isalnum() or c in " -_" else "" for c in name)
    return keep.strip().replace(" ", "_") + ".soundxpreset"


def main() -> None:
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    for name, overrides in PRESETS.items():
        unknown = set(overrides) - set(DEFAULTS)
        if unknown:
            raise SystemExit(f"{name}: unknown parameter ids {sorted(unknown)}")
        path = OUTPUT_DIR / safe_filename(name)
        path.write_text(preset_xml(overrides), encoding="utf-8")
        print(f"wrote {path.relative_to(OUTPUT_DIR.parent.parent)}")


if __name__ == "__main__":
    main()
