# SPDX-License-Identifier: GPL-3.0-only
"""Host checks và guest BIOS/UEFI cho VMM M10."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys

import pytest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from scripts.m07_image import (DISK_SIZE, ESP_OFFSET, ESP_SIZE, check_image,
                               create_image)
from scripts.qemu_harness import Config, run


def build_kernel(directory, fault="none"):
    subprocess.run([sys.executable, str(ROOT / "tests/m10/build_kernel.py"),
                    "--build", str(directory), "--fault", fault],
                   check=True, timeout=180, capture_output=True, text=True)
    return directory


def artifacts():
    bios = Path(os.environ.get("LAMIX_STAGE2_BUILD", ROOT / "build/m07-input"))
    uefi = Path(os.environ.get("LAMIX_UEFI_BUILD", ROOT / "build/m06"))
    if not (bios / "build.json").is_file():
        subprocess.run([sys.executable, str(ROOT / "tests/bios_stage2/build_stage2.py"),
                        "--build", str(bios), "--profile", "Debug"],
                       check=True, timeout=180)
    if not (uefi / "build.json").is_file():
        subprocess.run([sys.executable, str(ROOT / "tests/uefi/build_uefi.py"),
                        "--build", str(uefi)], check=True, timeout=180)
    return bios, uefi


def file_digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def kernel_digest(path):
    digest = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def image_fingerprints(image, bios, uefi, kernel_build):
    ranges = ((0, 512), (512, 512 + 512 + 16384),
              (1024 * 1024, 3 * 1024 * 1024),
              (ESP_OFFSET, ESP_OFFSET + ESP_SIZE),
              (DISK_SIZE - 33 * 512, DISK_SIZE))
    extents = []
    with Path(image).open("rb") as stream:
        for start, end in ranges:
            stream.seek(start)
            digest = hashlib.sha256()
            remaining = end - start
            while remaining:
                block = stream.read(min(1024 * 1024, remaining))
                assert block
                digest.update(block)
                remaining -= len(block)
            extents.append(digest.hexdigest())
    return {
        "image_extents": extents,
        "seabios": file_digest("/usr/share/seabios/bios.bin"),
        "ovmf_code": file_digest("/usr/share/OVMF/OVMF_CODE.fd"),
        "ovmf_vars_template": file_digest("/usr/share/OVMF/OVMF_VARS.fd"),
        "stage1": file_digest(bios / "stage1.bin"),
        "stage2": file_digest(bios / "stage2.bin"),
        "efi": file_digest(uefi / "BOOTX64.EFI"),
        "kernel": kernel_digest(kernel_build / "kernel.elf"),
    }


@pytest.mark.parametrize("fault", ["none", "ro", "nx", "guard"])
def test_kernel_m10_build_has_provenance(tmp_path, fault):
    directory = ROOT / "build" / f"m10-host-{fault}"
    build_kernel(directory, fault)
    report = json.loads((directory / "build.json").read_text())
    assert report["status"] == "PASS"
    assert report["mode"] == "m10-kernel"
    assert report["fault"] == fault
    assert report["hashes"]["kernel.elf"] == kernel_digest(directory / "kernel.elf")
    assert report["source_hashes"]["kernel/vmm.c"]
    assert report["source_hashes"]["kernel/vmm.h"]
    assert report["source_hashes"]["kernel/pmm.c"]
    inspection = (directory / "kernel.inspection.txt").read_text()
    assert "There are no relocations" in inspection or "Relocation section" not in inspection
    assert report["load_segments"] >= 2


def test_m07_image_accepts_m10_kernel_and_checks_payload(tmp_path):
    bios, uefi = artifacts()
    kernel_build = build_kernel(ROOT / "build/m10-image-kernel")
    image = tmp_path / "m10.img"
    created = create_image(image, bios, uefi, kernel_build=kernel_build,
                           seed="m10-image-profile")
    checked = check_image(image, bios, uefi, kernel_build=kernel_build)
    assert created["kernel_sha256"] == checked["kernel_sha256"]
    assert created["kernel_sha256"] == kernel_digest(kernel_build / "kernel.elf")
    assert "EFI/LAMIX/A/KERNEL.ELF" in checked["paths"]


@pytest.fixture(scope="session")
def m10_images(tmp_path_factory):
    bios, uefi = artifacts()
    root = tmp_path_factory.mktemp("m10-images")
    images = {}
    for fault in ("none", "ro", "nx", "guard"):
        kernel_build = build_kernel(ROOT / f"build/m10-guest-{fault}", fault)
        image = root / f"{fault}.img"
        created = create_image(image, bios, uefi, kernel_build=kernel_build,
                               seed=f"m10-{fault}-guest")
        checked = check_image(image, bios, uefi, kernel_build=kernel_build)
        assert created["kernel_sha256"] == checked["kernel_sha256"]
        images[fault] = (image, bios, uefi, kernel_build, created)
    return images


@pytest.mark.qemu
@pytest.mark.parametrize("profile,purpose", [("bios", "bios-stage2"),
                                              ("uefi", "uefi-loader")])
@pytest.mark.parametrize("fault", ["none", "ro", "nx", "guard"])
def test_m10_guest_boot_and_page_faults(m10_images, profile, purpose, fault):
    image, bios, uefi, kernel_build, build_report = m10_images[fault]
    before = image_fingerprints(image, bios, uefi, kernel_build)
    panic_marker = "M10:FAIL" if fault == "none" else "M10:PANIC"
    result = run(Config(profile=profile, purpose=purpose, image=image,
                        success_marker="M10:PASS",
                        panic_marker=panic_marker, timeout=35,
                        output_root=ROOT / "out/m10"))
    after = image_fingerprints(image, bios, uefi, kernel_build)
    serial_path = Path(result.directory) / "serial.log"
    serial = serial_path.read_text(errors="replace")
    report = {
        "status": result.status,
        "profile": profile,
        "fault": fault,
        "kernel_sha256": build_report["kernel_sha256"],
        "protected_before": before,
        "protected_after": after,
        "qemu": result.__dict__,
        "serial": serial,
    }
    output = ROOT / "out/m10/results"
    output.mkdir(parents=True, exist_ok=True)
    (output / f"{fault}-{profile}.json").write_text(
        json.dumps(report, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8")
    assert before == after, "QEMU đã sửa source image/firmware/artifact"
    assert not result.cleanup_forced and result.cleanup_error is None
    assert "M08:PASS\n" in serial and "M09:PASS\n" in serial
    assert "M10:VMM root=switched" in serial
    assert "M10:FAIL" not in serial
    if fault == "none":
        assert result.status == "PASS", result.detail
        assert "M10:PASS\n" in serial
        assert "M08:TICK\n" in serial
        return
    assert result.status == "PANIC", result.detail
    assert "M10:PANIC\n" in serial and "M08:PANIC\n" in serial
    panic_line = next(line for line in serial.splitlines()
                      if line.startswith("vector="))
    fields = dict(part.split("=", 1) for part in panic_line.split())
    assert fields["vector"] == "000000000000000e"
    if fault == "ro":
        assert int(fields["error"], 16) == 3
        assert int(fields["cr2"], 16) == 0xfffffc0000000000
    elif fault == "nx":
        assert int(fields["error"], 16) == 0x11
        assert int(fields["cr2"], 16) == 0xfffffc0000000000
    else:
        assert int(fields["error"], 16) == 2
        guard = next(line.split("=", 1)[1] for line in serial.splitlines()
                     if line.startswith("M08:GUARD="))
        assert fields["cr2"] == guard
