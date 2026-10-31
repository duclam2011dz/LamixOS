# SPDX-License-Identifier: GPL-3.0-only
"""M17 build provenance, shared runnable image, VirtIO-GPU guest and capture."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import struct
import zlib

import pytest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from scripts.m07_image import create_image, check_image, verify_build_report
from scripts.qemu_harness import Config, run
from scripts.m17_qemu import qemu_command
sys.path.insert(0, str(ROOT / "tests/m10"))
from test_m10 import image_fingerprints


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def build_kernel(directory):
    subprocess.run([sys.executable, str(ROOT / "tests/m17/build_kernel.py"),
                    "--build", str(directory)], check=True, timeout=300,
                   capture_output=True, text=True)
    return directory


def platform_artifacts():
    bios = Path(os.environ.get("LAMIX_STAGE2_BUILD", ROOT / "build/m17-bios"))
    uefi = Path(os.environ.get("LAMIX_UEFI_BUILD", ROOT / "build/m17-uefi"))
    bios_ok = False
    if (bios / "build.json").is_file():
        try:
            verify_build_report(bios, ("stage1.bin", "stage2.bin", "kernel.elf"))
            bios_ok = json.loads((bios / "build.json").read_text()).get("m16_video") is True
        except (OSError, ValueError, json.JSONDecodeError):
            bios_ok = False
    if not bios_ok:
        subprocess.run([sys.executable, str(ROOT / "tests/bios_stage2/build_stage2.py"),
                        "--build", str(bios), "--profile", "Release", "--m16-video"],
                       check=True, timeout=300, capture_output=True, text=True)
    uefi_ok = False
    if (uefi / "build.json").is_file():
        try:
            verify_build_report(uefi, ("BOOTX64.EFI",))
            uefi_ok = True
        except (OSError, ValueError, json.JSONDecodeError):
            uefi_ok = False
    if not uefi_ok:
        subprocess.run([sys.executable, str(ROOT / "tests/uefi/build_uefi.py"),
                        "--build", str(uefi)], check=True, timeout=300,
                       capture_output=True, text=True)
    return bios, uefi


@pytest.fixture(scope="session")
def m17_image():
    bios, uefi = platform_artifacts()
    kernel = build_kernel(ROOT / "build/m17-guest")
    kernel_report = verify_build_report(kernel, ("kernel.elf",))
    image_dir = ROOT / "out/m17"
    image_dir.mkdir(parents=True, exist_ok=True)
    image = image_dir / f"lamix-m17-{kernel_report['hashes']['kernel.elf'][:12]}.img"
    if image.exists():
        checked = check_image(image, bios, uefi, kernel_build=kernel)
        created = checked
    else:
        created = create_image(image, bios, uefi, kernel_build=kernel,
                               seed="lamix-m17-virtio-gpu-2d")
        checked = check_image(image, bios, uefi, kernel_build=kernel)
    assert created["kernel_sha256"] == checked["kernel_sha256"]
    return image, bios, uefi, kernel, created


def ppm_dimensions(path):
    data = Path(path).read_bytes()
    assert data.startswith(b"P6\n")
    cursor = 3
    tokens = []
    while len(tokens) < 3:
        while data[cursor:cursor + 1].isspace():
            cursor += 1
        if data[cursor:cursor + 1] == b"#":
            cursor = data.index(b"\n", cursor) + 1
            continue
        end = cursor
        while data[end:end + 1] and not data[end:end + 1].isspace():
            end += 1
        tokens.append(int(data[cursor:end]))
        cursor = end + 1
    width, height, maximum = tokens
    pixels = data[cursor:]
    assert maximum == 255 and len(pixels) == width * height * 3
    colors = set(zip(pixels[0::3], pixels[1::3], pixels[2::3]))
    assert len(colors) >= 4 and any(pixels)
    return width, height


def test_manual_qemu_commands_use_single_virtio_vga(tmp_path):
    image = tmp_path / "m17.img"
    bios = qemu_command("bios", image, tmp_path, "qemu-system-x86_64",
                        tmp_path / "seabios.bin", tmp_path / "OVMF_CODE.fd",
                        tmp_path / "OVMF_VARS.fd")
    assert bios[bios.index("-vga") + 1] == "none"
    assert bios[bios.index("-device") + 1] == "virtio-vga"
    assert bios[bios.index("-bios") + 1] == str(tmp_path / "seabios.bin")
    assert any("snapshot=on" in arg for arg in bios)
    assert "-display" not in bios

    code = tmp_path / "OVMF_CODE.fd"
    variables = tmp_path / "OVMF_VARS.template.fd"
    code.write_bytes(b"code")
    variables.write_bytes(b"variables")
    uefi = qemu_command("uefi", image, tmp_path, "qemu-system-x86_64",
                        tmp_path / "seabios.bin", code, variables)
    assert uefi[uefi.index("-vga") + 1] == "none"
    assert uefi[uefi.index("-device") + 1] == "virtio-vga"
    assert any("snapshot=on" in arg for arg in uefi)
    assert (tmp_path / "OVMF_VARS.fd").read_bytes() == b"variables"


def ppm_to_png(source):
    data = Path(source).read_bytes()
    cursor = 3
    tokens = []
    while len(tokens) < 3:
        while data[cursor:cursor + 1].isspace(): cursor += 1
        if data[cursor:cursor + 1] == b"#":
            cursor = data.index(b"\n", cursor) + 1
            continue
        end = cursor
        while data[end:end + 1] and not data[end:end + 1].isspace(): end += 1
        tokens.append(int(data[cursor:end])); cursor = end + 1
    width, height, maximum = tokens
    pixels = data[cursor:]
    assert maximum == 255 and len(pixels) == width * height * 3
    rows = b"".join(b"\0" + pixels[y * width * 3:(y + 1) * width * 3]
                    for y in range(height))
    def chunk(kind, payload):
        body = kind + payload
        return struct.pack(">I", len(payload)) + body + struct.pack(">I", zlib.crc32(body))
    png = (b"\x89PNG\r\n\x1a\n" +
           chunk(b"IHDR", struct.pack(">2I5B", width, height, 8, 2, 0, 0, 0)) +
           chunk(b"IDAT", zlib.compress(rows, 9)) + chunk(b"IEND", b""))
    destination = Path(source).with_suffix(".png")
    destination.write_bytes(png)
    return destination


def run_guest(image, profile, purpose, with_gpu):
    output = ROOT / "out/m17"
    image_commands = (["-vga", "none", "-device", "virtio-vga"]
                      if with_gpu else ["-vga", "std"])
    result = run(Config(profile=profile, purpose=purpose, image=image,
        success_marker="M17:PASS", panic_marker="M17:FAIL", timeout=120,
        output_root=output / "runs",
        qmp_screendump_marker="M17:READY" if with_gpu else None,
        qmp_screendump_path="{run_dir}/screen.ppm" if with_gpu else None,
        extra_args=image_commands))
    return result


def save_report(profile, with_gpu, build_report, result, before, after, serial):
    run_dir = Path(result.directory)
    screenshot = run_dir / "screen.ppm"
    screenshot_png = screenshot.with_suffix(".png")
    report = {
        "status": result.status,
        "profile": profile,
        "virtio_gpu": with_gpu,
        "kernel_sha256": build_report["kernel_sha256"],
        "protected_image_extents": before["image_extents"],
        "protected_before": before,
        "protected_after": after,
        "qemu": result.__dict__,
        "serial": serial,
        "screenshot": str(screenshot) if screenshot.is_file() else None,
        "screenshot_sha256": digest(screenshot) if screenshot.is_file() else None,
        "screenshot_png": str(screenshot_png) if screenshot_png.is_file() else None,
        "screenshot_png_sha256": digest(screenshot_png) if screenshot_png.is_file() else None,
    }
    destination = ROOT / "out/m17/results"
    destination.mkdir(parents=True, exist_ok=True)
    (destination / f"{profile}-{'gpu' if with_gpu else 'fallback'}.json").write_text(
        json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    return screenshot


@pytest.mark.qemu
@pytest.mark.parametrize("profile,purpose", [("bios", "bios-stage2"),
                                              ("uefi", "uefi-loader")])
def test_m17_virtio_gpu_guest_and_screenshot(m17_image, profile, purpose):
    image, bios, uefi, kernel, build_report = m17_image
    before = image_fingerprints(image, bios, uefi, kernel)
    result = run_guest(image, profile, purpose, True)
    after = image_fingerprints(image, bios, uefi, kernel)
    serial = (Path(result.directory) / "serial.log").read_text(errors="replace")
    screenshot = Path(result.directory) / "screen.ppm"
    if screenshot.is_file(): ppm_to_png(screenshot)
    save_report(profile, True, build_report, result, before, after, serial)
    assert before == after, "QEMU đã sửa image, firmware hoặc kernel đầu vào"
    assert not result.cleanup_forced and result.cleanup_error is None
    assert result.status == "PASS", result.detail
    for marker in ("M08:PASS\n", "M09:PASS\n", "M10:PASS\n", "M11:PASS\n",
                   "M12:PASS\n", "M13:PASS\n", "M14:PASS\n", "M17:PCI:PASS\n",
                   "M17:TRANSPORT:PASS\n", "M17:GPU:2D:PASS\n",
                   "M17:SCANOUT:PASS\n", "M17:FRAME:PASS checksum=", "M17:PASS\n"):
        assert marker in serial, marker
    assert "M15:SELFTEST:SKIPPED\n" in serial
    assert "M17:GPU:FALLBACK" not in serial
    assert screenshot.is_file() and screenshot.stat().st_size > 1024
    assert ppm_dimensions(screenshot) == (1024, 768)
    png = ppm_to_png(screenshot)
    assert png.is_file() and png.stat().st_size > 1024


@pytest.mark.qemu
@pytest.mark.parametrize("profile,purpose", [("bios", "bios-stage2"),
                                              ("uefi", "uefi-loader")])
def test_m17_fallback_without_virtio_device(m17_image, profile, purpose):
    image, bios, uefi, kernel, build_report = m17_image
    before = image_fingerprints(image, bios, uefi, kernel)
    result = run_guest(image, profile, purpose, False)
    after = image_fingerprints(image, bios, uefi, kernel)
    serial = (Path(result.directory) / "serial.log").read_text(errors="replace")
    save_report(profile, False, build_report, result, before, after, serial)
    assert before == after
    assert not result.cleanup_forced and result.cleanup_error is None
    assert result.status == "PASS", result.detail
    assert "M17:GPU:FALLBACK:LBP_FRAMEBUFFER\n" in serial
    assert "M17:FRAME:PASS checksum=" in serial and "M17:PASS\n" in serial
    assert "M17:PCI:PASS\n" not in serial


def test_m17_kernel_provenance():
    directory = build_kernel(ROOT / "build/m17-host")
    report = json.loads((directory / "build.json").read_text(encoding="utf-8"))
    assert report["status"] == "PASS" and report["mode"] == "m17-kernel"
    assert report["hashes"]["kernel.elf"] == digest(directory / "kernel.elf")
    assert "kernel/virtio_gpu.c" in report["source_hashes"]
