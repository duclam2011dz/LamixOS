# SPDX-License-Identifier: GPL-3.0-only
"""M21 ABI build provenance và integration guest thật BIOS/UEFI."""
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


def build_m21():
    output = ROOT / "build/m21-guest"
    subprocess.run([sys.executable, str(ROOT / "tests/m21/build_kernel.py"),
                    "--build", str(output)], check=True, timeout=300,
                   capture_output=True, text=True)
    return output


def test_m21_build_provenance():
    output = build_m21()
    report = verify_build_report(output, ("kernel.elf",))
    assert report["status"] == "PASS" and report["mode"] == "m21-kernel"
    assert report["hashes"]["kernel.elf"] == digest(output / "kernel.elf")
    for source in ("kernel/syscall.c", "kernel/syscall.h",
                   "kernel/syscall_entry.asm", "user/syscall.c",
                   "user/include/lamix/syscall.h", "tests/m21/user_payload.c"):
        assert source in report["source_hashes"]
    for name in ("user_test.bin", "user_loop.bin", "user_invalid_return.bin"):
        assert 0 < (output / name).stat().st_size <= 4096


@pytest.fixture(scope="session")
def m21_artifacts():
    bios, uefi = platform_artifacts()
    kernel = build_m21()
    report = verify_build_report(kernel, ("kernel.elf",))
    bios_report = verify_build_report(bios, ("stage1.bin", "stage2.bin", "kernel.elf"))
    uefi_report = verify_build_report(uefi, ("BOOTX64.EFI",))
    image = ROOT / "out/m21" / (
        f"lamix-m21-{report['hashes']['kernel.elf'][:12]}-"
        f"{bios_report['hashes']['stage2.bin'][:8]}-"
        f"{uefi_report['hashes']['BOOTX64.EFI'][:8]}.img")
    image.parent.mkdir(parents=True, exist_ok=True)
    if image.exists():
        created = check_image(image, bios, uefi, kernel_build=kernel)
    else:
        created = create_image(image, bios, uefi, kernel_build=kernel,
                               seed="lamix-m21-syscall-abi-v1")
    checked = check_image(image, bios, uefi, kernel_build=kernel)
    assert created["kernel_sha256"] == checked["kernel_sha256"]
    return image, bios, uefi, kernel, report


@pytest.mark.qemu
@pytest.mark.parametrize("profile,purpose", [("bios", "bios-stage2"),
                                              ("uefi", "uefi-loader")])
@pytest.mark.parametrize("attempt", range(1, 4))
def test_m21_syscall_ring3(m21_artifacts, profile, purpose, attempt):
    image, bios, uefi, kernel, build_report = m21_artifacts
    before = image_fingerprints(image, bios, uefi, kernel)
    result = run(Config(profile=profile, purpose=purpose, image=image,
        success_marker="M21:PASS", panic_marker="M21:FAIL", timeout=180,
        output_root=ROOT / "out/m21/runs", extra_args=["-vga", "none",
                                                       "-device", "virtio-vga"]))
    run_dir = Path(result.directory)
    serial = (run_dir / "serial.log").read_text(errors="replace")
    after = image_fingerprints(image, bios, uefi, kernel)
    record = {"status": result.status, "profile": profile, "attempt": attempt,
              "kernel_sha256": build_report["hashes"]["kernel.elf"],
              "before": before, "after": after, "qemu": result.__dict__,
              "serial": serial}
    report_dir = ROOT / "out/m21/results"
    report_dir.mkdir(parents=True, exist_ok=True)
    (report_dir / f"{profile}-{attempt}.json").write_text(
        json.dumps(record, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    assert before == after, "QEMU đã thay đổi input được bảo vệ"
    assert result.status == "PASS", result.detail
    assert not result.cleanup_forced and result.cleanup_error is None
    for marker in ("M20:PROCESS:READY\n", "M20:RING3:ENTERING\n",
                   "M21:SYSCALL_ENTRY:READY\n", "M21:USER:ABI:PASS\n",
                   "M21:RETURN_VALIDATION:PASS\n", "M21:FAULT_ISOLATION:PASS\n",
                   "M21:IRETQ_FALLBACK:PASS\n", "M21:KILL_WAIT:PASS\n",
                   "M21:PASS\n", "M20:PASS\n"):
        assert marker in serial, marker
