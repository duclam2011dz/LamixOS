# SPDX-License-Identifier: GPL-3.0-only
"""M19 host provenance và guest scheduler BSP qua BIOS/UEFI."""
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys

import pytest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from scripts.m07_image import create_image, check_image, verify_build_report
from scripts.qemu_harness import Config, run
sys.path.insert(0, str(ROOT / "tests/m17"))
from test_m17 import platform_artifacts
sys.path.insert(0, str(ROOT / "tests/m10"))
from test_m10 import image_fingerprints


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def m19_artifacts():
    bios, uefi = platform_artifacts()
    kernel = ROOT / "build/m19-guest"
    subprocess.run([sys.executable, str(ROOT / "tests/m19/build_kernel.py"),
                    "--build", str(kernel)], check=True, timeout=300,
                   capture_output=True, text=True)
    report = verify_build_report(kernel, ("kernel.elf",))
    assert report["mode"] == "m19-kernel"
    out = ROOT / "out/m19"
    out.mkdir(parents=True, exist_ok=True)
    image = out / f"lamix-m19-{report['hashes']['kernel.elf'][:12]}.img"
    if image.exists():
        image_report = check_image(image, bios, uefi, kernel_build=kernel)
    else:
        image_report = create_image(image, bios, uefi, kernel_build=kernel,
                                    seed="lamix-m19-bsp-scheduler")
        image_report = check_image(image, bios, uefi, kernel_build=kernel)
    return image, bios, uefi, kernel, report, image_report


@pytest.fixture(scope="session")
def artifacts():
    return m19_artifacts()


def test_m19_kernel_provenance(artifacts):
    _, _, _, kernel, report, _ = artifacts
    assert report["status"] == "PASS"
    assert report["hashes"]["kernel.elf"] == digest(kernel / "kernel.elf")
    assert "kernel/scheduler.c" in report["source_hashes"]
    assert "LAMIX_M18" not in " ".join(
        " ".join(command) for command in report["commands"])


@pytest.mark.qemu
@pytest.mark.parametrize("profile,purpose", [("bios", "bios-stage2"),
                                              ("uefi", "uefi-loader")])
@pytest.mark.parametrize("attempt", range(1, 4))
def test_m19_preemptive_guest(artifacts, profile, purpose, attempt):
    image, bios, uefi, kernel, build_report, _ = artifacts
    before = image_fingerprints(image, bios, uefi, kernel)
    result = run(Config(profile=profile, purpose=purpose, image=image,
        success_marker="M19:PASS", panic_marker="M08:PANIC", timeout=120,
        output_root=ROOT / "out/m19/runs", extra_args=["-vga", "none",
                                                       "-device", "virtio-vga"]))
    run_dir = Path(result.directory)
    serial = (run_dir / "serial.log").read_text(errors="replace")
    after = image_fingerprints(image, bios, uefi, kernel)
    report = {"status": result.status, "attempt": attempt, "profile": profile,
              "kernel_sha256": build_report["hashes"]["kernel.elf"],
              "image_protected_extents": before["image_extents"],
              "before": before, "after": after,
              "qemu": result.__dict__, "serial": serial}
    report_dir = ROOT / "out/m19/results"
    report_dir.mkdir(parents=True, exist_ok=True)
    (report_dir / f"{profile}-{attempt}.json").write_text(
        json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    assert before == after, "QEMU đã thay đổi input được bảo vệ"
    assert result.status == "PASS", result.detail
    assert not result.cleanup_forced and result.cleanup_error is None
    for marker in ("M17:PASS\n", "M19:SCHEDULER:READY\n",
                   "M19:PREEMPT:RUNNING\n", "M19:PREEMPT:PASS ticks=",
                   "M19:RR:PASS\n", "M19:AGING:PASS ticks=",
                   "M19:MUTEX:PI_HANDOFF:PASS\n", "M19:WAKEUP:PASS\n",
                   "M19:PASS\n"):
        assert marker in serial, marker
    progress = re.search(r"M19:PREEMPT:PASS ticks=([0-9a-f]+) worker0=([0-9a-f]+) worker1=([0-9a-f]+)", serial)
    assert progress and int(progress.group(1), 16) >= 40
    assert int(progress.group(2), 16) > 0 and int(progress.group(3), 16) > 0
