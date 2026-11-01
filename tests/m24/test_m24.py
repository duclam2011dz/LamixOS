# SPDX-License-Identifier: GPL-3.0-only
"""M24 SMP build provenance và guest BIOS/UEFI đa topology."""
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


def build_m24():
    output = ROOT / "build/m24-guest"
    subprocess.run([sys.executable, str(ROOT / "tests/m24/build_kernel.py"),
                    "--build", str(output), "--max-cpus", "16"],
                   check=True, timeout=900, capture_output=True, text=True)
    return output


def build_m24_fault(name, max_cpus=16):
    output = ROOT / f"build/m24-fault-{name}"
    subprocess.run([sys.executable, str(ROOT / "tests/m24/build_kernel.py"),
                    "--build", str(output), "--max-cpus", str(max_cpus),
                    "--fault", name], check=True, timeout=900,
                   capture_output=True, text=True)
    return output


def test_m24_build_provenance():
    output = build_m24()
    report = verify_build_report(output, ("kernel.elf",))
    assert report["status"] == "PASS" and report["mode"] == "m24-kernel"
    for name in ("kernel.elf", "initramfs.cpio", "alpha.elf", "beta.elf"):
        assert report["hashes"][name] == digest(output / name)
    assert report["max_cpus"] == 16


@pytest.fixture(scope="session")
def artifacts():
    bios, uefi = platform_artifacts()
    kernel = build_m24()
    report = verify_build_report(kernel, ("kernel.elf",))
    br = verify_build_report(bios, ("stage1.bin", "stage2.bin", "kernel.elf"))
    ur = verify_build_report(uefi, ("BOOTX64.EFI",))
    image = ROOT / "out/m24" / (
        f"lamix-m24-{report['hashes']['kernel.elf'][:12]}-"
        f"{br['hashes']['stage2.bin'][:8]}-"
        f"{ur['hashes']['BOOTX64.EFI'][:8]}.img")
    image.parent.mkdir(parents=True, exist_ok=True)
    if image.exists():
        result = check_image(image, bios, uefi, kernel_build=kernel)
    else:
        result = create_image(image, bios, uefi, kernel_build=kernel,
                              seed="lamix-m24-smp")
    assert result["kernel_sha256"] == check_image(
        image, bios, uefi, kernel_build=kernel)["kernel_sha256"]
    return image, bios, uefi, kernel, report


@pytest.mark.qemu
@pytest.mark.parametrize("profile,purpose", [("bios", "bios-stage2"),
                                               ("uefi", "uefi-loader")])
@pytest.mark.parametrize("cpus", (1, 2, 4, 8))
@pytest.mark.parametrize("attempt", range(1, 4))
def test_m24_smp_guest(artifacts, profile, purpose, cpus, attempt):
    image, bios, uefi, kernel, report = artifacts
    before = image_fingerprints(image, bios, uefi, kernel)
    result = run(Config(profile=profile, purpose=purpose, image=image,
                        success_marker="M23:PASS", panic_marker="M24:FAIL",
                        timeout=240, output_root=ROOT / "out/m24/runs",
                        cpu_count=cpus,
                        extra_args=["-vga", "none", "-device", "virtio-vga"]))
    after = image_fingerprints(image, bios, uefi, kernel)
    assert before == after and result.status == "PASS"
    assert not result.cleanup_forced and result.cleanup_error is None
    serial = (Path(result.directory) / "serial.log").read_text(errors="replace")
    for marker in ("M24:TOPOLOGY:PASS", "M24:TLB_SHOOTDOWN:PASS",
                   "M24:WORK_STEALING:PASS",
                   "M23:APP_ALPHA:PASS\n", "M23:APP_BETA:PASS\n",
                   "M23:PROCESS_ISOLATION:PASS\n", "M23:CLEANUP:PASS\n",
                   "M24:PASS\n"):
        assert marker in serial, marker
    online = serial.count("M24:AP_START:ONLINE") + 1
    assert online == cpus
    observed = next(line for line in serial.splitlines()
                    if line.startswith("M24:USER_CPU_MASK="))
    mask = int(observed.split("=", 1)[1].split()[0], 16)
    timer_line = next(line for line in serial.splitlines()
                      if line.startswith("M24:TIMER_COUNTS="))
    timer_counts = [int(value, 16) for value in timer_line.split("=", 1)[1].split(",")]
    assert len(timer_counts) == cpus and timer_counts[0] > 0
    if cpus > 1:
        assert any(timer_counts[1:])
        assert mask.bit_count() >= 2 and "STEALS=0x0" not in observed


@pytest.mark.qemu
@pytest.mark.parametrize("name,max_cpus,cpus,marker", (
    ("ap-timeout", 16, 2,
     "M24:AP_START:TIMEOUT apic=0000000000000001"),
    ("none", 2, 4,
     "M24:TOPOLOGY:UNSUPPORTED count=0000000000000004 x2apic=0000000000000000"),
    ("tlb-ack-timeout", 16, 2,
     "M24:TLB_SHOOTDOWN:TIMEOUT va=fffff50000000000"),
))
def test_m24_fault_injection(name, max_cpus, cpus, marker):
    bios, uefi = platform_artifacts()
    kernel = build_m24_fault(name, max_cpus=max_cpus)
    report = verify_build_report(kernel, ("kernel.elf",))
    assert report["status"] == "PASS" and report["mode"] == "m24-kernel"
    assert report["max_cpus"] == max_cpus
    bios_report = verify_build_report(
        bios, ("stage1.bin", "stage2.bin", "kernel.elf"))
    uefi_report = verify_build_report(uefi, ("BOOTX64.EFI",))
    output = ROOT / "out/m24/faults"
    output.mkdir(parents=True, exist_ok=True)
    image = output / (
        f"{name}-{report['hashes']['kernel.elf'][:12]}-"
        f"{bios_report['hashes']['stage2.bin'][:8]}-"
        f"{uefi_report['hashes']['BOOTX64.EFI'][:8]}.img")
    if image.exists():
        check_image(image, bios, uefi, kernel_build=kernel)
    else:
        create_image(image, bios, uefi, kernel_build=kernel,
                     seed=f"lamix-m24-fault-{name}")
    before = image_fingerprints(image, bios, uefi, kernel)
    result = run(Config(profile="bios", purpose="bios-stage2", image=image,
                        success_marker=marker, panic_marker="M08:PANIC",
                        timeout=120, output_root=ROOT / "out/m24/fault-runs",
                        cpu_count=cpus, extra_args=["-vga", "none", "-device",
                                                     "virtio-vga"]))
    after = image_fingerprints(image, bios, uefi, kernel)
    assert before == after and result.status == "PASS", result.detail
    assert not result.cleanup_forced and result.cleanup_error is None
    serial = Path(result.directory, "serial.log").read_text(errors="replace")
    assert marker in serial and "M08:PANIC" not in serial
