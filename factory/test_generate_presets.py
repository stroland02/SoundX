"""pytest validation of the factory preset bank."""

import xml.etree.ElementTree as ET

from generate_presets import DEFAULTS, PRESETS, RANGES, preset_xml


def test_every_default_has_a_range():
    assert set(DEFAULTS) == set(RANGES)


def test_presets_only_override_known_params():
    for name, overrides in PRESETS.items():
        unknown = set(overrides) - set(DEFAULTS)
        assert not unknown, f"{name}: {unknown}"


def test_all_values_inside_ranges():
    for name, overrides in PRESETS.items():
        params = DEFAULTS | overrides
        for pid, value in params.items():
            lo, hi = RANGES[pid]
            assert lo <= float(value) <= hi, f"{name}: {pid}={value} outside [{lo}, {hi}]"


def test_xml_is_well_formed_and_complete():
    for name, overrides in PRESETS.items():
        root = ET.fromstring(preset_xml(overrides))
        assert root.tag == "PARAMS"
        ids = {el.get("id") for el in root.findall("PARAM")}
        assert ids == set(DEFAULTS), f"{name}: missing {set(DEFAULTS) - ids}"


def test_bank_has_a_useful_size():
    assert len(PRESETS) >= 12
