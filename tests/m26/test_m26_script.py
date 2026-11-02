# SPDX-License-Identifier: GPL-3.0-only
from pathlib import Path
import sys
import pytest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from scripts.lamix_qemu import LATEST_PROFILE, resolve_profile

def test_latest_remains_m25_until_m26_acceptance():
    assert LATEST_PROFILE == "m25"
    assert resolve_profile("latest") == "m25"
    assert resolve_profile("m26") == "m26"

def test_m26_requires_explicit_profile():
    assert resolve_profile("m26") == "m26"
