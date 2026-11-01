# SPDX-License-Identifier: GPL-3.0-only
"""Kiểm tra an toàn lựa chọn profile và tạo raw test disk M25."""
from pathlib import Path
import stat
import sys

import pytest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from scripts.lamix_qemu import LATEST_PROFILE, resolve_profile
sys.path.insert(0, str(ROOT / "tests/m25"))
from test_m25 import create_test_disk, verify_test_disk


def test_latest_remains_m24_until_m25_acceptance():
    assert LATEST_PROFILE == "m24"
    assert resolve_profile("latest") == "m24"
    assert resolve_profile("m25") == "m25"


def test_test_disk_is_regular_and_confined(tmp_path):
    root = ROOT / "out/m25/runs"
    root.mkdir(parents=True, exist_ok=True)
    with __import__("tempfile").TemporaryDirectory(prefix="host-test-", dir=root) as temp:
        disk = create_test_disk(Path(temp) / "disk.raw")
        assert stat.S_ISREG(disk.stat().st_mode)
        assert disk.stat().st_size == 128 * 512
        verify_test_disk(disk, expected_only=False)
    with pytest.raises(ValueError):
        create_test_disk(tmp_path / "outside.raw")


def test_test_disk_rejects_existing_symlink(tmp_path):
    root = ROOT / "out/m25/runs"
    root.mkdir(parents=True, exist_ok=True)
    with __import__("tempfile").TemporaryDirectory(prefix="host-test-", dir=root) as temp:
        target = Path(temp) / "target.raw"
        target.write_bytes(bytes(128 * 512))
        link = Path(temp) / "link.raw"
        link.symlink_to(target)
        with pytest.raises(ValueError):
            create_test_disk(link)
