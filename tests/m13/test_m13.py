# SPDX-License-Identifier: GPL-3.0-only
"""Build, provenance, image và guest QEMU M13."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys

import pytest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
sys.path.insert(0, str(ROOT / "tests/m10"))
from scripts.m07_image import create_image, check_image
from scripts.qemu_harness import Config, run
from test_m10 import artifacts, image_fingerprints


def build_kernel(directory):
    subprocess.run([sys.executable, str(ROOT / "tests/m13/build_kernel.py"),
                    "--build", str(directory)], check=True, timeout=180,
                   capture_output=True, text=True)
    return directory


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def test_m13_kernel_provenance():
    directory = build_kernel(ROOT / "build/m13-host")
    report = json.loads((directory / "build.json").read_text(encoding="utf-8"))
    assert report["status"] == "PASS" and report["mode"] == "m13-kernel"
    assert report["hashes"]["kernel.elf"] == digest(directory / "kernel.elf")
    for source in ("kernel/acpi.c", "kernel/pci.c", "kernel/device.c",
                   "tests/m13/build_kernel.py"):
        assert report["source_hashes"][source]
    assert report["load_segments"] >= 2
    inspection = (directory / "kernel.inspection.txt").read_text(encoding="utf-8")
    assert "There are no relocations" in inspection or "Relocation section" not in inspection


@pytest.fixture(scope="session")
def m13_image(tmp_path_factory):
    bios = Path(os.environ.get("LAMIX_STAGE2_BUILD", ROOT / "build/m07-input"))
    uefi = Path(os.environ.get("LAMIX_UEFI_BUILD", ROOT / "build/m06"))
    if not (bios / "build.json").is_file() or not (uefi / "build.json").is_file():
        bios, uefi = artifacts()
    kernel = build_kernel(ROOT / "build/m13-guest")
    image = tmp_path_factory.mktemp("m13-image") / "shared.img"
    created = create_image(image, bios, uefi, kernel_build=kernel,
                           seed="m13-acpi-pci-device")
    checked = check_image(image, bios, uefi, kernel_build=kernel)
    assert created["kernel_sha256"] == checked["kernel_sha256"]
    return image, bios, uefi, kernel, created


@pytest.mark.qemu
@pytest.mark.parametrize("profile,purpose", [("bios", "bios-stage2"),
                                              ("uefi", "uefi-loader")])
def test_m13_guest_enumerates_platform(m13_image, profile, purpose):
    image, bios, uefi, kernel, build_report = m13_image
    before = image_fingerprints(image, bios, uefi, kernel)
    result = run(Config(profile=profile, purpose=purpose, image=image,
                        success_marker="M13:PASS", panic_marker="M13:FAIL",
                        timeout=60, output_root=ROOT / "out/m13"))
    after = image_fingerprints(image, bios, uefi, kernel)
    serial = (Path(result.directory) / "serial.log").read_text(errors="replace")
    report = {"status": result.status, "profile": profile,
              "kernel_sha256": build_report["kernel_sha256"],
              "protected_before": before, "protected_after": after,
              "qemu": result.__dict__, "serial": serial}
    out = ROOT / "out/m13/results"
    out.mkdir(parents=True, exist_ok=True)
    (out / f"{profile}.json").write_text(
        json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    assert before == after, "QEMU đã sửa image, firmware hoặc kernel đầu vào"
    assert not result.cleanup_forced and result.cleanup_error is None
    assert result.status == "PASS", result.detail
    for marker in ("M08:PASS\n", "M09:PASS\n", "M10:PASS\n",
                   "M11:PASS\n", "M12:PASS\n", "M13:PASS\n",
                   "M13:PCI:PASS", "M13:DEVICE:PASS"):
        assert marker in serial
    assert "M13:ACPI:INVALID" not in serial
    assert "mmio=mapped" in serial
    assert "M13:FAIL" not in serial
