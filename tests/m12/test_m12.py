# SPDX-License-Identifier: GPL-3.0-only
"""M12 Buddy PMM build/provenance và guest BIOS/UEFI."""
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
    subprocess.run([sys.executable, str(ROOT / "tests/m12/build_kernel.py"),
                    "--build", str(directory)], check=True, timeout=180,
                   capture_output=True, text=True)
    return directory


def file_digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def test_m12_kernel_profile_has_buddy_provenance(tmp_path):
    directory = build_kernel(ROOT / "build/m12-host")
    report = json.loads((directory / "build.json").read_text(encoding="utf-8"))
    assert report["status"] == "PASS"
    assert report["mode"] == "m12-kernel"
    assert report["hashes"]["kernel.elf"] == file_digest(directory / "kernel.elf")
    for source in ("kernel/pmm.c", "kernel/pmm.h", "tests/m12/build_kernel.py"):
        assert report["source_hashes"][source]
    assert report["load_segments"] >= 2
    assert report["hashes"]["kernel.elf"] == file_digest(directory / "kernel.elf")
    inspection = (directory / "kernel.inspection.txt").read_text(encoding="utf-8")
    assert "There are no relocations" in inspection or "Relocation section" not in inspection


@pytest.fixture(scope="session")
def m12_image(tmp_path_factory):
    bios = Path(os.environ.get("LAMIX_STAGE2_BUILD", ROOT / "build/m07-input"))
    uefi = Path(os.environ.get("LAMIX_UEFI_BUILD", ROOT / "build/m06"))
    if not (bios / "build.json").is_file() or not (uefi / "build.json").is_file():
        from test_m10 import artifacts as ensure_artifacts
        bios, uefi = ensure_artifacts()
    kernel = build_kernel(ROOT / "build/m12-guest")
    image = tmp_path_factory.mktemp("m12-image") / "shared.img"
    created = create_image(image, bios, uefi, kernel_build=kernel,
                           seed="m12-buddy-pmm")
    checked = check_image(image, bios, uefi, kernel_build=kernel)
    assert created["kernel_sha256"] == checked["kernel_sha256"]
    assert created["kernel_sha256"] == file_digest(kernel / "kernel.elf")
    return image, bios, uefi, kernel, created


@pytest.mark.qemu
@pytest.mark.parametrize("profile,purpose", [("bios", "bios-stage2"),
                                              ("uefi", "uefi-loader")])
def test_m12_buddy_guest_boots_same_kernel(m12_image, profile, purpose):
    image, bios, uefi, kernel, build_report = m12_image
    before = image_fingerprints(image, bios, uefi, kernel)
    result = run(Config(profile=profile, purpose=purpose, image=image,
                        success_marker="M12:PASS", panic_marker="M12:FAIL",
                        timeout=60, output_root=ROOT / "out/m12"))
    after = image_fingerprints(image, bios, uefi, kernel)
    serial = (Path(result.directory) / "serial.log").read_text(errors="replace")
    report = {"status": result.status, "profile": profile, "purpose": purpose,
              "kernel_sha256": build_report["kernel_sha256"],
              "protected_before": before, "protected_after": after,
              "qemu": result.__dict__, "serial": serial}
    output = ROOT / "out/m12/results"
    output.mkdir(parents=True, exist_ok=True)
    (output / f"{profile}.json").write_text(
        json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    assert before == after, "QEMU đã sửa image, firmware hoặc kernel đầu vào"
    assert not result.cleanup_forced and result.cleanup_error is None
    assert result.status == "PASS", result.detail
    for marker in ("M08:PASS\n", "M09:PASS\n", "M10:PASS\n",
                   "M11:PASS\n", "M12:PASS\n"):
        assert marker in serial
    assert "M12:FAIL" not in serial
    buddy = next(line for line in serial.splitlines()
                 if line.startswith("M12:BUDDY "))
    fields = dict(part.split("=", 1) for part in buddy.split()[1:])
    assert int(fields["order"], 16) >= 0
    assert int(fields["metadata"], 16) == 589824
