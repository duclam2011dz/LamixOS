# SPDX-License-Identifier: GPL-3.0-only
"""M27 shell/service guest and profile checks."""
import hashlib
import os
from pathlib import Path
import stat
import subprocess
import sys
import uuid

import pytest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from scripts.lamix_qemu import LATEST_PROFILE, build_image, resolve_profile
from scripts.m07_image import DISK_SIZE, SECTOR, identities, metadata
from scripts.qemu_harness import Config, qemu_value, run
sys.path.insert(0, str(ROOT / "tests/m17"))
from test_m17 import platform_artifacts
sys.path.insert(0, str(ROOT / "tests/m10"))
from test_m10 import image_fingerprints


def digest(path):
    h = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def safe_disk_path(path):
    root = (ROOT / "out/m27/runs").resolve()
    path = Path(path)
    if path.is_symlink():
        raise ValueError("test image không được là symlink")
    parent = path.parent.resolve(strict=True)
    if parent != root and root not in parent.parents:
        raise ValueError("test image phải nằm dưới out/m27/runs/")
    return path


def create_user_data_disk(path):
    path = safe_disk_path(path)
    fd = os.open(path, os.O_CREAT | os.O_EXCL | os.O_RDWR |
                 getattr(os, "O_NOFOLLOW", 0), 0o600)
    try:
        if not stat.S_ISREG(os.fstat(fd).st_mode):
            raise ValueError("M27 chỉ cho phép regular-file test image")
        os.ftruncate(fd, DISK_SIZE)
        os.fsync(fd)
    finally:
        os.close(fd)
    spec, pmbr, primary, entries, backup = metadata(
        identities("lamix-m27-user-data"))
    start, end = spec["partitions"][4]
    with path.open("r+b", buffering=0) as disk:
        def write(lba, data):
            disk.seek(lba * SECTOR)
            disk.write(data)
        write(0, pmbr)
        write(1, primary)
        write(2, entries)
        write(spec["backup_array"], entries)
        write(spec["blocks"] - 1, backup)
        disk.flush()
        os.fsync(disk.fileno())
    subprocess.run(["mkfs.fat", "-F", "32", "-S", "512", "-s", "8",
                    "-h", str(start), "--invariant", "--offset", str(start),
                    str(path), str((end - start + 1) // 2)],
                   check=True, timeout=300, stdout=subprocess.PIPE,
                   stderr=subprocess.PIPE)
    return path


def protected_ranges(path):
    spec = metadata(identities("lamix-m27-user-data"))[0]
    ranges = [(0, 34 * SECTOR),
              ((spec["blocks"] - 33) * SECTOR, DISK_SIZE)]
    esp_start, esp_end = spec["partitions"][1]
    ranges.append((esp_start * SECTOR, (esp_end + 1) * SECTOR))
    for start, end in spec["partitions"][2:4]:
        ranges.extend(((start * SECTOR, (start + 8) * SECTOR),
                       ((end - 7) * SECTOR, (end + 1) * SECTOR)))
    data_start, data_end = spec["partitions"][4]
    ranges.append(((data_end - 7) * SECTOR, (data_end + 1) * SECTOR))
    hashes = []
    with Path(path).open("rb") as disk:
        for start, end in ranges:
            disk.seek(start)
            h = hashlib.sha256()
            remain = end - start
            while remain:
                chunk = disk.read(min(remain, 1024 * 1024))
                if not chunk:
                    raise ValueError("GPT/ESP guard range bị cắt")
                h.update(chunk)
                remain -= len(chunk)
            hashes.append(h.hexdigest())
    return hashes


def console_script():
    codes = {" ": "spc", "/": "slash", ".": "dot", "-": "minus"}
    commands = ["help"]
    script = []
    for command in commands:
        for character in command + "\n":
            code = codes.get(character, character)
            if character == "\n":
                code = "ret"
            for down in (True, False):
                event = {"type": "key", "data": {"key": {
                    "type": "qcode", "data": code}, "down": down}}
                script.append({"execute": "input-send-event",
                               "arguments": {"events": [event]}})
                script.append({"delay_ms": 30 if down else 30})
    return script


def serial_console_script():
    return [("M27:SHELL:PS2:PASS", "\n".join(("write /data/a x",
             "cat /data/a", "ls /data", "alpha a",
             "service restart shell")) + "\n", 2.0),
            ("M27:CONSOLE:RUN2_READY", "service stop shell\n"),
            ("M27:INIT:SERVICE:INPUT_READY", "service start shell\n"),
            ("M27:CONSOLE:RUN3_READY", "exit\n")]


@pytest.fixture(scope="session")
def m27_artifacts():
    output = ROOT / "build/m27-guest"
    kernel_build = output / "kernel"
    subprocess.run([sys.executable, str(ROOT / "tests/m27/build_kernel.py"),
                    "--build", str(kernel_build), "--max-cpus", "16"],
                   check=True, timeout=1800, capture_output=True, text=True)
    bios, uefi = platform_artifacts()
    image = build_image("m27", output)
    report = __import__("json").loads((kernel_build / "build.json").read_text())
    return image, bios, uefi, kernel_build, report


def test_profile_gate_and_explicit_selection():
    assert LATEST_PROFILE == "m26"
    assert resolve_profile("latest") == "m26"
    assert resolve_profile("m27") == "m27"


@pytest.mark.qemu
@pytest.mark.parametrize("firmware,purpose", [("bios", "bios-stage2"),
                                                ("uefi", "uefi-loader")])
@pytest.mark.parametrize("attempt", (1, 2, 3))
def test_m27_shell_files_and_clean_reboot(m27_artifacts, firmware, purpose,
                                          attempt):
    image, bios, uefi, kernel, report = m27_artifacts
    boot_before = image_fingerprints(image, bios, uefi, kernel)
    root = ROOT / "out/m27/runs"
    root.mkdir(parents=True, exist_ok=True)
    run_dir = root / f"{firmware}-{attempt}-{uuid.uuid4().hex}"
    run_dir.mkdir(mode=0o700)
    disk = create_user_data_disk(run_dir / "m27-user-data-gpt.raw")
    initial = digest(disk)
    guards = protected_ranges(disk)
    extra = ["-drive", f"if=none,id=m27data,file={qemu_value(disk)},format=raw,cache=writeback",
             "-device", "virtio-blk-pci,drive=m27data"]
    serials = []
    for boot in (1, 2):
        result = run(Config(profile=firmware, purpose=purpose, image=image,
                            success_marker="M27:SMOKE:PASS", panic_marker="M27:FAIL",
                            timeout=360, output_root=run_dir / f"boot-{boot}",
                            cpu_count=2, extra_args=extra,
                            qmp_trigger_marker="M27:CONSOLE:INPUT_READY",
                                qmp_script=console_script(), serial_pipe=True,
                            qmp_screendump_marker="M27:SHELL:FILES:PASS",
                            qmp_screendump_path=Path("{run_dir}") / "terminal.ppm",
                            serial_input_script=serial_console_script()))
        assert result.status == "PASS", result.detail
        assert not result.cleanup_forced and result.cleanup_error is None
        log = (Path(result.directory) / "serial.log").read_text(errors="replace")
        for marker in ("M27:VFS:READY", "M27:INIT:GRAPH:PASS",
                       "M27:INIT:SERVICE:REAP:PASS",
                       "M27:CONSOLE:READY",
                       "M27:SHELL:PS2:PASS",
                       "M27:SHELL:COMMANDS:PASS",
                       "M27:SHELL:FILES:PASS", "M23:APP_ALPHA:PASS",
                       "M27:CAPABILITY:DENY:PASS",
                       "M27:CAPABILITY:GRANT:DENY:PASS",
                       "M27:INIT:SERVICE:RESTARTED",
                       "M27:CONSOLE:RUN2_READY",
                       "M27:INIT:SERVICE:INPUT_READY",
                       "M27:CONSOLE:RUN3_READY",
                       "M27:SMOKE:PASS\n"):
            assert marker in log, marker
        ppm = (Path(result.directory) / "terminal.ppm").read_bytes()
        header, pixels = ppm.split(b"\n255\n", 1)
        assert header.startswith(b"P6\n1024 768")
        assert len(pixels) == 1024 * 768 * 3
        assert len(set(pixels[::max(1, len(pixels) // 4096)])) > 2
        serials.append(log)
        assert image_fingerprints(image, bios, uefi, kernel) == boot_before
        assert protected_ranges(disk) == guards
    final = digest(disk)
    assert final != initial
    result_dir = ROOT / "out/m27/results"
    result_dir.mkdir(parents=True, exist_ok=True)
    (result_dir / f"{firmware}-{attempt}-{uuid.uuid4().hex}.json").write_text(
        __import__("json").dumps({"status": "PASS", "firmware": firmware,
          "cpus": 2, "attempt": attempt, "kernel_sha256": report["hashes"]["kernel.elf"],
          "data_disk_sha256_before": initial, "data_disk_sha256_after": final,
          "protected_ranges_sha256": guards, "boot_image_firmware_fingerprints": boot_before,
          "serial_boot_1": serials[0], "serial_boot_2": serials[1]},
          ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


@pytest.mark.qemu
@pytest.mark.parametrize("firmware,purpose", [("bios", "bios-stage2"),
                                                ("uefi", "uefi-loader")])
@pytest.mark.parametrize("attempt", (1, 2, 3))
def test_m27_reboot_command_persists(m27_artifacts, firmware, purpose, attempt):
    image, bios, uefi, kernel, report = m27_artifacts
    root = ROOT / "out/m27/runs"
    root.mkdir(parents=True, exist_ok=True)
    run_dir = root / f"reboot-{firmware}-{attempt}-{uuid.uuid4().hex}"
    run_dir.mkdir(mode=0o700)
    disk = create_user_data_disk(run_dir / "m27-user-data-gpt.raw")
    initial = digest(disk)
    guards = protected_ranges(disk)
    extra = ["-drive", f"if=none,id=m27data,file={qemu_value(disk)},format=raw,cache=writeback",
             "-device", "virtio-blk-pci,drive=m27data"]
    serial_input = "write /data/M27-REBOOT.TXT durable\nreboot\n"
    result = run(Config(profile=firmware, purpose=purpose, image=image,
        success_marker="M27:REBOOT:PERSIST:PASS", panic_marker="M27:FAIL",
        timeout=240, output_root=run_dir / "boot", cpu_count=2,
        no_reboot=False, extra_args=extra,
        qmp_trigger_marker="M27:CONSOLE:INPUT_READY",
        qmp_script=console_script(), serial_pipe=True,
        serial_input_script=[("M27:SHELL:PS2:PASS", serial_input, 2.0)]))
    assert result.status == "PASS", result.detail
    assert not result.cleanup_forced and result.cleanup_error is None
    log = (Path(result.directory) / "serial.log").read_text(errors="replace")
    assert "M27:REBOOT:PERSIST:PASS\n" in log
    assert digest(disk) != initial
    assert protected_ranges(disk) == guards
    result_dir = ROOT / "out/m27/results"
    result_dir.mkdir(parents=True, exist_ok=True)
    (result_dir / f"reboot-{firmware}-{attempt}-{uuid.uuid4().hex}.json").write_text(
        __import__("json").dumps({"status": "PASS", "firmware": firmware,
            "attempt": attempt, "kernel_sha256": report["hashes"]["kernel.elf"],
            "data_disk_sha256_before": initial, "data_disk_sha256_after": digest(disk),
            "protected_ranges_sha256": guards,
            "serial": log}, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def test_disk_builder_rejects_existing_and_outside(tmp_path):
    root = ROOT / "out/m27/runs"
    root.mkdir(parents=True, exist_ok=True)
    run_dir = root / f"host-{uuid.uuid4().hex}"
    run_dir.mkdir(mode=0o700)
    disk = create_user_data_disk(run_dir / "disk.raw")
    assert disk.stat().st_size == DISK_SIZE
    with pytest.raises(FileExistsError):
        create_user_data_disk(disk)
    with pytest.raises(ValueError):
        create_user_data_disk(tmp_path / "outside.raw")
