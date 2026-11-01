# SPDX-License-Identifier: GPL-3.0-only
"""M20 build provenance và guest ring 3/process isolation."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys

import pytest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from scripts.m07_image import check_image, create_image, verify_build_report
from scripts.qemu_harness import Config, run
sys.path.insert(0, str(ROOT / "tests/m17"))
from test_m17 import platform_artifacts
sys.path.insert(0, str(ROOT / "tests/m10"))
from test_m10 import image_fingerprints


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def build_m20():
    output = ROOT / "build/m20-guest"
    subprocess.run([sys.executable, str(ROOT / "tests/m20/build_kernel.py"),
                    "--build", str(output)], check=True, timeout=300,
                   capture_output=True, text=True)
    return output


def test_m20_build_provenance():
    output = build_m20()
    report = verify_build_report(output, ("kernel.elf",))
    assert report["status"] == "PASS" and report["mode"] == "m20-kernel"
    assert report["hashes"]["kernel.elf"] == digest(output / "kernel.elf")
    assert "kernel/process.c" in report["source_hashes"]
    assert "kernel/vmm.c" in report["source_hashes"]
    assert "kernel/scheduler.c" in report["source_hashes"]


@pytest.fixture(scope="session")
def m20_artifacts():
    bios, uefi = platform_artifacts()
    kernel = build_m20()
    report = verify_build_report(kernel, ("kernel.elf",))
    bios_report = verify_build_report(bios, ("stage1.bin", "stage2.bin", "kernel.elf"))
    uefi_report = verify_build_report(uefi, ("BOOTX64.EFI",))
    image = ROOT / "out/m20" / (
        f"lamix-m20-{report['hashes']['kernel.elf'][:12]}-"
        f"{bios_report['hashes']['stage2.bin'][:8]}-"
        f"{uefi_report['hashes']['BOOTX64.EFI'][:8]}.img")
    image.parent.mkdir(parents=True, exist_ok=True)
    if image.exists():
        created = check_image(image, bios, uefi, kernel_build=kernel)
    else:
        created = create_image(image, bios, uefi, kernel_build=kernel,
                               seed="lamix-m20-process-isolation")
    checked = check_image(image, bios, uefi, kernel_build=kernel)
    assert created["kernel_sha256"] == checked["kernel_sha256"]
    return image, bios, uefi, kernel, report


@pytest.mark.qemu
@pytest.mark.parametrize("profile,purpose", [("bios", "bios-stage2"),
                                              ("uefi", "uefi-loader")])
@pytest.mark.parametrize("attempt", range(1, 4))
def test_m20_ring3_process_isolation(m20_artifacts, profile, purpose, attempt):
    image, bios, uefi, kernel, build_report = m20_artifacts
    before = image_fingerprints(image, bios, uefi, kernel)
    result = run(Config(profile=profile, purpose=purpose, image=image,
        success_marker="M20:PASS", panic_marker="M20:FAIL", timeout=180,
        output_root=ROOT / "out/m20/runs", extra_args=["-vga", "none",
                                                       "-device", "virtio-vga"]))
    run_dir = Path(result.directory)
    serial = (run_dir / "serial.log").read_text(errors="replace")
    after = image_fingerprints(image, bios, uefi, kernel)
    record = {"status": result.status, "profile": profile, "attempt": attempt,
              "kernel_sha256": build_report["hashes"]["kernel.elf"],
              "before": before, "after": after, "qemu": result.__dict__,
              "serial": serial}
    report_dir = ROOT / "out/m20/results"
    report_dir.mkdir(parents=True, exist_ok=True)
    (report_dir / f"{profile}-{attempt}.json").write_text(
        json.dumps(record, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    assert before == after, "QEMU đã thay đổi input được bảo vệ"
    assert result.status == "PASS", result.detail
    assert not result.cleanup_forced and result.cleanup_error is None
    for marker in ("M17:PASS\n", "M20:PROCESS:READY\n", "M20:RING3:ENTERING\n",
                   "M20:USER_FAULT:EXPECTED\n", "M20:ISOLATION:PASS\n",
                   "M20:USER_FAULT:PASS\n", "M20:KILL_WAIT:PASS\n", "M20:PASS\n"):
        assert marker in serial, marker
