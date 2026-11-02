# SPDX-License-Identifier: GPL-3.0-only
"""M25 host safety and VirtIO-Blk guest verification."""
import hashlib
import os
from pathlib import Path
import stat
import subprocess
import sys
import tempfile
import uuid

import pytest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from scripts.m07_image import check_image, create_image, verify_build_report
from scripts.qemu_harness import Config, qemu_value, run
sys.path.insert(0, str(ROOT / "tests/m17"))
from test_m17 import platform_artifacts
sys.path.insert(0, str(ROOT / "tests/m10"))
from test_m10 import image_fingerprints

DISK_BYTES = 128 * 512
TEST_SECTORS = (8, 10, 127)


def digest(path):
    h = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def _safe_run_path(path):
    root = (ROOT / "out/m25/runs").resolve()
    path = Path(path)
    if path.is_symlink():
        raise ValueError("test disk không được là symlink")
    parent = path.parent.resolve(strict=True)
    if parent != root and root not in parent.parents:
        raise ValueError("test disk phải nằm dưới out/m25/runs/")
    return path


def create_test_disk(path):
    path = _safe_run_path(path)
    flags = os.O_CREAT | os.O_EXCL | os.O_RDWR
    if hasattr(os, "O_NOFOLLOW"):
        flags |= os.O_NOFOLLOW
    descriptor = os.open(path, flags, 0o600)
    try:
        if not stat.S_ISREG(os.fstat(descriptor).st_mode):
            raise ValueError("M25 chỉ cho phép regular-file test disk")
        os.ftruncate(descriptor, DISK_BYTES)
        os.fsync(descriptor)
    finally:
        os.close(descriptor)
    return path


def expected_sector(sector):
    if sector == 8:
        return bytes((i ^ 0x5a) & 0xff for i in range(512))
    if sector == 10:
        return bytes(((i * 3) ^ 0xa5) & 0xff for i in range(512))
    if sector == 127:
        return bytes(((i * 7) ^ 0x3c) & 0xff for i in range(512))
    return bytes(512)


def verify_test_disk(path, expected_only=True):
    path = _safe_run_path(path)
    info = path.lstat()
    if not stat.S_ISREG(info.st_mode) or info.st_size != DISK_BYTES:
        raise ValueError("test disk phải là regular file có đúng 128 sector")
    with path.open("rb", buffering=0) as stream:
        for sector in range(DISK_BYTES // 512):
            stream.seek(sector * 512)
            actual = stream.read(512)
            expected = expected_sector(sector) if expected_only else bytes(512)
            if actual != expected:
                raise AssertionError(f"sector {sector} sai hoặc guard sector đổi")
    return digest(path)


def _build_m25():
    output = ROOT / "build/m25-guest"
    kernel = output / "kernel"
    subprocess.run([sys.executable, str(ROOT / "tests/m25/build_kernel.py"),
                    "--build", str(kernel), "--max-cpus", "16"],
                   check=True, timeout=1200, capture_output=True, text=True)
    report = verify_build_report(kernel, ("kernel.elf",))
    assert report["mode"] == "m25-kernel" and report.get("m25_block") is True
    return kernel, report


@pytest.fixture(scope="session")
def m25_artifacts():
    bios, uefi = platform_artifacts()
    kernel, report = _build_m25()
    bios_report = verify_build_report(bios, ("stage1.bin", "stage2.bin", "kernel.elf"))
    uefi_report = verify_build_report(uefi, ("BOOTX64.EFI",))
    image_dir = ROOT / "out/m25"
    image_dir.mkdir(parents=True, exist_ok=True)
    image = image_dir / (f"lamix-m25-{report['hashes']['kernel.elf'][:12]}-"
                         f"{bios_report['hashes']['stage2.bin'][:8]}-"
                         f"{uefi_report['hashes']['BOOTX64.EFI'][:8]}.img")
    if image.exists():
        check_image(image, bios, uefi, kernel_build=kernel)
    else:
        create_image(image, bios, uefi, kernel_build=kernel,
                     seed="lamix-m25-virtio-blk")
    return image, bios, uefi, kernel, report


@pytest.mark.qemu
@pytest.mark.parametrize("firmware,purpose", [("bios", "bios-stage2"),
                                                ("uefi", "uefi-loader")])
@pytest.mark.parametrize("attempt", (1, 2, 3))
def test_m25_virtio_block_guest(m25_artifacts, firmware, purpose, attempt):
    image, bios, uefi, kernel, build_report = m25_artifacts
    before = image_fingerprints(image, bios, uefi, kernel)
    run_root = ROOT / "out/m25/runs"
    run_root.mkdir(parents=True, exist_ok=True)
    run_dir = run_root / f"{firmware}-{attempt}-{uuid.uuid4().hex}"
    run_dir.mkdir(mode=0o700)
    disk = create_test_disk(run_dir / "test-disk.raw")
    initial_hash = digest(disk)
    extras = ["-vga", "none", "-device", "virtio-vga",
              "-drive", f"if=none,id=m25test,file={qemu_value(disk)},format=raw,cache=writeback",
              "-device", "virtio-blk-pci,drive=m25test"]
    result = run(Config(profile=firmware, purpose=purpose, image=image,
                        success_marker="M25:PASS", panic_marker="M25:FAIL",
                        timeout=240, output_root=run_dir / "qemu", cpu_count=2,
                        extra_args=extras))
    after = image_fingerprints(image, bios, uefi, kernel)
    assert before == after, "boot image, firmware hoặc OVMF template đã đổi"
    assert result.status == "PASS", result.detail
    assert not result.cleanup_forced and result.cleanup_error is None
    serial = (Path(result.directory) / "serial.log").read_text(errors="replace")
    for marker in ("M25:DEVICE:READY", "M25:ASYNC_CALLBACK:PASS",
                   "M25:SYNC_IO:PASS", "M25:BOUNDARY:PASS",
                   "M25:FLUSH:PASS", "M25:PASS\n", "M24:PASS\n",
                   "M23:PASS\n"):
        assert marker in serial, marker
    after_hash = verify_test_disk(disk)
    assert after_hash != initial_hash
    report_dir = ROOT / "out/m25/results"
    report_dir.mkdir(parents=True, exist_ok=True)
    report = {"status": "PASS", "firmware": firmware, "attempt": attempt,
              "cpus": 2, "kernel_sha256": build_report["hashes"]["kernel.elf"],
              "test_disk_sha256_before": initial_hash,
              "test_disk_sha256_after": after_hash,
              "boot_image_firmware_fingerprints_before": before,
              "boot_image_firmware_fingerprints_after": after,
              "serial": serial, "qemu": result.__dict__}
    (report_dir / f"{firmware}-{attempt}-{uuid.uuid4().hex}.json").write_text(
        __import__("json").dumps(report, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8")


def test_create_disk_rejects_outside_and_links(tmp_path):
    root = ROOT / "out/m25/runs"
    root.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="host-test-", dir=root) as temp:
        destination = Path(temp) / "disk.raw"
        create_test_disk(destination)
        verify_test_disk(destination, expected_only=False)
        with pytest.raises(FileExistsError):
            create_test_disk(destination)
        link = Path(temp) / "link.raw"
        link.symlink_to(destination)
        with pytest.raises(ValueError):
            create_test_disk(link)
    with pytest.raises(ValueError):
        create_test_disk(tmp_path / "outside.raw")
