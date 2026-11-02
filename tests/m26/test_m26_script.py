# SPDX-License-Identifier: GPL-3.0-only
from pathlib import Path
import sys
import pytest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from scripts.lamix_qemu import LATEST_PROFILE, resolve_profile

def test_latest_selects_m26_candidate_and_keeps_m25_available():
    assert LATEST_PROFILE == "m26"
    assert resolve_profile("latest") == "m26"
    assert resolve_profile("m25") == "m25"
    assert resolve_profile("m26") == "m26"
