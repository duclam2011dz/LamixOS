# SPDX-License-Identifier: GPL-3.0-only
"""Host provenance/image checks và guest BIOS/UEFI cho heap M11."""
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
    subprocess.run([sys.executable, str(ROOT / "tests/m11/build_kernel.py"),
                    "--build", str(directory)], check=True, timeout=180,
                   capture_output=True, text=True)
    return directory


def file_digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def test_m11_kernel_has_provenance_and_cpp_object(tmp_path):
    directory = build_kernel(ROOT / "build/m11-host")
    report = json.loads((directory / "build.json").read_text(encoding="utf-8"))
    assert report["status"] == "PASS"
    assert report["mode"] == "m11-kernel"
    assert report["hashes"]["kernel.elf"] == file_digest(directory / "kernel.elf")
    assert report["source_hashes"]["kernel/heap.c"]
    assert report["source_hashes"]["kernel/heap.h"]
    assert report["source_hashes"]["tests/m11/cpp_probe.cpp"]
    assert any("x86_64-elf-g++" in " ".join(command)
               for command in report["commands"])
    inspection = (directory / "kernel.inspection.txt").read_text(encoding="utf-8")
    assert "There are no relocations" in inspection or "Relocation section" not in inspection
    assert report["load_segments"] >= 2


@pytest.fixture(scope="session")
def m11_image(tmp_path_factory):
    bios = Path(os.environ.get("LAMIX_STAGE2_BUILD", ROOT / "build/m07-input"))
    uefi = Path(os.environ.get("LAMIX_UEFI_BUILD", ROOT / "build/m06"))
    if not (bios / "build.json").is_file() or not (uefi / "build.json").is_file():
        bios, uefi = artifacts()
    kernel = build_kernel(ROOT / "build/m11-guest")
    image = tmp_path_factory.mktemp("m11-image") / "shared.img"
    created = create_image(image, bios, uefi, kernel_build=kernel,
                           seed="m11-kernel-heap")
    checked = check_image(image, bios, uefi, kernel_build=kernel)
    assert created["kernel_sha256"] == checked["kernel_sha256"]
    assert created["kernel_sha256"] == file_digest(kernel / "kernel.elf")
    return image, bios, uefi, kernel, created


@pytest.mark.qemu
@pytest.mark.parametrize("profile,purpose", [("bios", "bios-stage2"),
                                              ("uefi", "uefi-loader")])
def test_m11_guest_heap_on_shared_kernel(m11_image, profile, purpose):
    image, bios, uefi, kernel, build_report = m11_image
    before = image_fingerprints(image, bios, uefi, kernel)
    result = run(Config(profile=profile, purpose=purpose, image=image,
                        success_marker="M11:PASS", panic_marker="M11:FAIL",
                        timeout=45, output_root=ROOT / "out/m11"))
    after = image_fingerprints(image, bios, uefi, kernel)
    serial = (Path(result.directory) / "serial.log").read_text(errors="replace")
    report = {
        "status": result.status,
        "profile": profile,
        "kernel_sha256": build_report["kernel_sha256"],
        "protected_before": before,
        "protected_after": after,
        "qemu": result.__dict__,
        "serial": serial,
    }
    output = ROOT / "out/m11/results"
    output.mkdir(parents=True, exist_ok=True)
    (output / f"{profile}.json").write_text(
        json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    assert before == after, "QEMU đã sửa image, firmware hoặc kernel đầu vào"
    assert not result.cleanup_forced and result.cleanup_error is None
    assert result.status == "PASS", result.detail
    for marker in ("M08:PASS\n", "M09:PASS\n", "M10:PASS\n",
                   "M11:PASS\n"):
        assert marker in serial
    assert "M11:FAIL" not in serial
