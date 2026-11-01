# SPDX-License-Identifier: GPL-3.0-only
"""M23 ELF loader/runtime provenance và guest BIOS/UEFI."""
import hashlib
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


def build_m23():
    output = ROOT / "build/m23-guest"
    subprocess.run([sys.executable, str(ROOT / "tests/m23/build_kernel.py"),
                    "--build", str(output)], check=True, timeout=600,
                   capture_output=True, text=True)
    return output


def test_m23_build_provenance():
    output = build_m23()
    report = verify_build_report(output, ("kernel.elf",))
    assert report["status"] == "PASS" and report["mode"] == "m23-kernel"
    for name in ("kernel.elf", "initramfs.cpio", "alpha.elf", "beta.elf"):
        assert report["hashes"][name] == digest(output / name)
    for app in ("alpha.elf", "beta.elf"):
        inspection = (output / app.replace(".elf", ".inspection.txt")).read_text()
        assert any(line.strip().startswith("Type:") and line.split()[1] == "DYN"
                   for line in inspection.splitlines())
        assert "INTERP" not in inspection and "NEEDED" not in inspection


@pytest.fixture(scope="session")
def artifacts():
    bios, uefi = platform_artifacts()
    kernel = build_m23()
    report = verify_build_report(kernel, ("kernel.elf",))
    br = verify_build_report(bios, ("stage1.bin", "stage2.bin", "kernel.elf"))
    ur = verify_build_report(uefi, ("BOOTX64.EFI",))
    image = ROOT / "out/m23" / (
        f"lamix-m23-{report['hashes']['kernel.elf'][:12]}-"
        f"{br['hashes']['stage2.bin'][:8]}-{ur['hashes']['BOOTX64.EFI'][:8]}.img")
    image.parent.mkdir(parents=True, exist_ok=True)
    if image.exists():
        result = check_image(image, bios, uefi, kernel_build=kernel)
    else:
        result = create_image(image, bios, uefi, kernel_build=kernel,
                              seed="lamix-m23-static-elf")
    assert result["kernel_sha256"] == check_image(
        image, bios, uefi, kernel_build=kernel)["kernel_sha256"]
    return image, bios, uefi, kernel, report


@pytest.mark.qemu
@pytest.mark.parametrize("profile,purpose", [("bios", "bios-stage2"),
                                               ("uefi", "uefi-loader")])
@pytest.mark.parametrize("attempt", range(1, 4))
def test_m23_static_elf_guest(artifacts, profile, purpose, attempt):
    image, bios, uefi, kernel, report = artifacts
    before = image_fingerprints(image, bios, uefi, kernel)
    result = run(Config(profile=profile, purpose=purpose, image=image,
                        success_marker="M23:PASS", panic_marker="M23:FAIL",
                        timeout=180, output_root=ROOT / "out/m23/runs",
                        extra_args=["-vga", "none", "-device", "virtio-vga"]))
    after = image_fingerprints(image, bios, uefi, kernel)
    assert before == after and result.status == "PASS"
    assert not result.cleanup_forced and result.cleanup_error is None
    serial = (Path(result.directory) / "serial.log").read_text(errors="replace")
    for marker in ("M23:MALFORMED_ELF:PASS\n", "M20:RING3:ENTERING\n",
                   "M23:APP_ALPHA:PASS\n",
                   "M23:APP_BETA:PASS\n", "M23:PROCESS_ISOLATION:PASS\n",
                   "M23:CLEANUP:PASS\n", "M23:PASS\n"):
        assert marker in serial, marker
