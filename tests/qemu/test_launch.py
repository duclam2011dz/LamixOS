# SPDX-License-Identifier: GPL-3.0-only
"""QEMU thật paused; không thực thi firmware hoặc chứng minh Lamix OS boot."""

import hashlib
import os
from pathlib import Path

import pytest

from scripts.qemu_harness import Config, run


@pytest.mark.qemu
@pytest.mark.parametrize("profile", ["bios", "uefi"])
def test_real_paused_launch(profile):
    paths = [Path("/usr/share/seabios/bios.bin")] if profile == "bios" else [Path("/usr/share/OVMF/OVMF_CODE.fd"), Path("/usr/share/OVMF/OVMF_VARS.fd")]
    before = {path: hashlib.sha256(path.read_bytes()).hexdigest() for path in paths}
    result = run(Config(profile=profile, launch_only=True))
    assert result.status == "PASS", result
    assert result.mode == "launch-only" and result.returncode == 0
    assert result.backend == "qemu"
    assert not result.cleanup_forced
    with pytest.raises(ChildProcessError):
        os.waitpid(result.pid, os.WNOHANG)
    with pytest.raises(ProcessLookupError):
        os.killpg(result.pid, 0)
    assert before == {path: hashlib.sha256(path.read_bytes()).hexdigest() for path in paths}
