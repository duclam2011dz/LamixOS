# SPDX-License-Identifier: GPL-3.0-only
"""Kiểm tra dispatcher script QEMU chung và giữ tương thích profile cũ."""
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from scripts.lamix_qemu import LATEST_PROFILE, resolve_profile


def test_latest_profile_tracks_current_milestone():
    assert LATEST_PROFILE == "m24"
    assert resolve_profile("latest") == "m24"
    assert resolve_profile("m17") == "m17"
    assert resolve_profile("m19") == "m19"
    assert resolve_profile("m20") == "m20"
    assert resolve_profile("m24") == "m24"
