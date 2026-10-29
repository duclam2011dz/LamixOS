# SPDX-License-Identifier: GPL-3.0-only
"""M06 host PE assertions and opt-in QEMU/OVMF guest execution."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys

import pytest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
sys.path.insert(0, str(ROOT / "tests/bios_stage2"))
sys.path.insert(0, str(ROOT / "tests/uefi"))
from scripts.qemu_harness import Config, run
from stage2_fixture import create_stage2_image, hashes
from build_uefi import build


def checked_build(path):
    path = Path(path)
    report_path = path / "build.json"
    if not report_path.exists():
        build(path)
    report = json.loads(report_path.read_text(encoding="utf-8"))
    assert report["status"] == "PASS"
    for name, digest in report["source_hashes"].items():
        assert hashlib.sha256((ROOT / name).read_bytes()).hexdigest() == digest, name
    return report


def stage2_build():
    path = Path(os.environ.get("LAMIX_STAGE2_BUILD", ROOT / "build/m05"))
    report_path = path / "build.json"
    if not report_path.exists():
        subprocess.run([sys.executable, str(ROOT / "tests/bios_stage2/build_stage2.py"),
                        "--build", str(path)], check=True)
    report = json.loads(report_path.read_text(encoding="utf-8"))
    assert report["status"] == "PASS", "M05 kernel build phải PASS"
    for name, digest in report["source_hashes"].items():
        assert hashlib.sha256((ROOT / name).read_bytes()).hexdigest() == digest, name
    for name, digest in report["hashes"].items():
        assert hashlib.sha256((path / name).read_bytes()).hexdigest() == digest, name
    return path


def test_pe_application():
    report = checked_build(Path(os.environ.get("LAMIX_UEFI_BUILD", ROOT / "build/m06")))
    pe = report["pe"]
    assert pe["machine"] == 0x8664 and pe["magic"] == 0x20B
    assert pe["subsystem"] == 10 and pe["imports_rva"] == 0
    assert pe["relocations"]["size"] > 0 and pe["entry_rva"] > 0
    assert report["hashes"]["BOOTX64.EFI"] == hashlib.sha256(
        (Path(os.environ.get("LAMIX_UEFI_BUILD", ROOT / "build/m06")) /
         "BOOTX64.EFI").read_bytes()).hexdigest()


@pytest.mark.parametrize("variant,reason", [
    ("valid", None), ("missing-a", "KERNEL_FILE"),
    ("elf-magic", "ELF"), ("note-required-acpi", "ELF"),
    ("gpt-guid", "GPT_IDENTITY"),
])
@pytest.mark.qemu
def test_ovmf_guest(tmp_path, variant, reason):
    efi_build = Path(os.environ.get("LAMIX_UEFI_BUILD", ROOT / "build/m06"))
    checked_build(efi_build)
    build05 = stage2_build()
    image = tmp_path / "m06.img"
    manifest = create_stage2_image(image, build05, variant=variant,
                                   efi_application=efi_build / "BOOTX64.EFI")
    protected = hashes(image, manifest)
    code = Path("/usr/share/OVMF/OVMF_CODE.fd")
    variables = Path("/usr/share/OVMF/OVMF_VARS.fd")
    firmware_hashes = [hashlib.sha256(p.read_bytes()).hexdigest() for p in (code, variables)]
    result = run(Config(profile="uefi", purpose="uefi-loader", image=image,
                        success_marker="M06:PASS", panic_marker="M06:FAIL",
                        timeout=30, output_root=ROOT / "out/m06"))
    serial = (Path(result.directory) / "serial.log").read_text(errors="replace").splitlines()
    assert result.mode == "uefi-loader" and result.returncode == 0, (result, serial)
    assert not result.cleanup_forced and not result.cleanup_error
    assert hashes(image, manifest) == protected
    assert firmware_hashes == [hashlib.sha256(p.read_bytes()).hexdigest() for p in (code, variables)]
    case = {"variant": variant, "expected": "PANIC" if reason else "PASS",
            "actual": result.status, "reason": reason, "serial": serial,
            "firmware_sha256": firmware_hashes, "protected_extents": protected,
            "cleanup_forced": result.cleanup_forced, "cleanup_error": result.cleanup_error,
            "command": result.command}
    (Path(result.directory) / "case.json").write_text(json.dumps(case, indent=2) + "\n")
    if reason:
        assert result.status == "PANIC" and "M06:FAIL" in serial
        assert serial[serial.index("M06:FAIL") + 1] == reason
    else:
        assert "M06:FRAMEBUFFER" in serial
        assert result.status == "PASS" and serial[-1] == "M06:PASS"


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--build-only", type=Path)
    parser.add_argument("--guest", action="store_true")
    args, _ = parser.parse_known_args()
    if args.build_only:
        build(args.build_only)
