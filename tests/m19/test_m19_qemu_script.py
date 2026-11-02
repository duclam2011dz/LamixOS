# SPDX-License-Identifier: GPL-3.0-only
"""Kiểm tra dispatcher script QEMU chung và giữ tương thích profile cũ."""
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from scripts.lamix_qemu import LATEST_PROFILE, resolve_profile


def test_latest_profile_tracks_current_milestone():
    assert resolve_profile("latest") == LATEST_PROFILE
    assert LATEST_PROFILE in {"m19", "m20", "m21", "m22", "m23", "m24", "m25", "m26"}
    assert resolve_profile("m17") == "m17"
    assert resolve_profile("m19") == "m19"
    assert resolve_profile("m20") == "m20"
    assert resolve_profile("m24") == "m24"
    assert resolve_profile("m25") == "m25"
    assert resolve_profile("m26") == "m26"
