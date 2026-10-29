# SPDX-License-Identifier: GPL-3.0-only
"""Host image checks và boot parity M07 trên cùng một kernel ELF."""
import errno
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import zlib

import pytest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
sys.path.insert(0, str(ROOT / "tests/m07"))
from scripts.m07_image import (DISK_SIZE, ESP_OFFSET, ESP_SIZE, check_image,
                               create_image, identities, metadata)
from scripts.qemu_harness import Config, run
from gpt_model import validate as validate_gpt


def artifact_builds():
    bios = Path(os.environ.get("LAMIX_STAGE2_BUILD", ROOT / "build/m07-input"))
    uefi = Path(os.environ.get("LAMIX_UEFI_BUILD", ROOT / "build/m06"))
    if not (bios / "build.json").is_file():
        subprocess.run([sys.executable, str(ROOT / "tests/bios_stage2/build_stage2.py"),
                        "--build", str(bios)], check=True, timeout=120)
    if not (uefi / "build.json").is_file():
        subprocess.run([sys.executable, str(ROOT / "tests/uefi/build_uefi.py"),
                        "--build", str(uefi)], check=True, timeout=180)
    return bios, uefi


def file_hash(path):
    h = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def protected_hashes(image, bios, uefi):
    extents = [(0, 512), (512, 512 + 512 + 16384),
               (DISK_SIZE - 33 * 512, DISK_SIZE),
               (1024 * 1024, 3 * 1024 * 1024),
               (ESP_OFFSET, ESP_OFFSET + ESP_SIZE)]
    values = []
    with Path(image).open("rb") as stream:
        for start, end in extents:
            stream.seek(start)
            h = hashlib.sha256()
            remaining = end - start
            while remaining:
                block = stream.read(min(1024 * 1024, remaining))
                if not block:
                    raise AssertionError("image bị cắt trong protected extent")
                h.update(block)
                remaining -= len(block)
            values.append(h.hexdigest())
    firmware = [Path("/usr/share/seabios/bios.bin"),
                Path("/usr/share/OVMF/OVMF_CODE.fd"),
                Path("/usr/share/OVMF/OVMF_VARS.fd")]
    return {"extents": values, "firmware": [file_hash(p) for p in firmware],
            "stage1": file_hash(bios / "stage1.bin"),
            "stage2": file_hash(bios / "stage2.bin"),
            "kernel": file_hash(bios / "kernel.elf"),
            "efi": file_hash(uefi / "BOOTX64.EFI")}


def sparse_clone(source, target):
    """Sao chép các extents đã cấp phát, không đọc các lỗ của image 8 GiB."""
    with Path(source).open("rb") as src, Path(target).open("xb") as dst:
        dst.truncate(Path(source).stat().st_size)
        source_fd, target_fd = src.fileno(), dst.fileno()
        position = 0
        while position < Path(source).stat().st_size:
            try:
                start = os.lseek(source_fd, position, os.SEEK_DATA)
            except OSError as exc:
                if exc.errno == errno.ENXIO:
                    break
                raise
            end = os.lseek(source_fd, start, os.SEEK_HOLE)
            cursor = start
            while cursor < end:
                block = os.pread(source_fd, min(1024 * 1024, end - cursor), cursor)
                if not block:
                    raise AssertionError("không đọc được image extent")
                os.pwrite(target_fd, block, cursor)
                cursor += len(block)
            position = end
        os.fsync(target_fd)


def kernel_payload_offset(image):
    with Path(image).open("rb") as disk:
        disk.seek(ESP_OFFSET)
        bpb = disk.read(512)
        spc, reserved, fats = bpb[13], int.from_bytes(bpb[14:16], "little"), bpb[16]
        fat_sectors = int.from_bytes(bpb[36:40], "little")
        data_start = reserved + fats * fat_sectors
        root = int.from_bytes(bpb[44:48], "little")

        def next_cluster(cluster):
            disk.seek(ESP_OFFSET + reserved * 512 + cluster * 4)
            return int.from_bytes(disk.read(4), "little") & 0x0fffffff

        def find(directory, name, is_directory):
            cluster = directory
            while cluster < 0x0ffffff8:
                start = data_start + (cluster - 2) * spc
                for sector in range(spc):
                    disk.seek(ESP_OFFSET + (start + sector) * 512)
                    data = disk.read(512)
                    for offset in range(0, 512, 32):
                        entry = data[offset:offset + 32]
                        if not entry or entry[0] == 0:
                            break
                        if entry[0] in (0xe5,) or entry[11] == 0x0f or entry[11] & 8:
                            continue
                        if entry[:11] == name:
                            if bool(entry[11] & 16) != is_directory:
                                raise AssertionError("FAT payload entry có loại sai")
                            found = (int.from_bytes(entry[20:22], "little") << 16) | \
                                int.from_bytes(entry[26:28], "little")
                            return found, int.from_bytes(entry[28:32], "little")
                cluster = next_cluster(cluster)
            raise AssertionError("thiếu FAT entry: " + name.decode("ascii"))

        cluster = root
        for component in (b"EFI        ", b"LAMIX      ", b"A          "):
            cluster, _ = find(cluster, component, True)
        cluster, size = find(cluster, b"KERNEL  ELF", False)
        if size <= 0:
            raise AssertionError("kernel FAT file rỗng")
        return ESP_OFFSET + (data_start + (cluster - 2) * spc) * 512


@pytest.fixture(scope="session")
def m07_image(tmp_path_factory):
    bios, uefi = artifact_builds()
    image = tmp_path_factory.mktemp("m07-image") / "lamix.img"
    report = create_image(image, bios, uefi, seed="m07-tests-v1")
    checked = check_image(image, bios, uefi)
    assert report["kernel_sha256"] == checked["kernel_sha256"]
    return image, report, bios, uefi


def test_guid_seed_and_gpt_profile():
    first = identities("repeatable")
    assert first == identities("repeatable")
    assert len(set(first)) == 6 and all(value.int for value in first)
    assert first != identities("different")
    random_a, random_b = identities(None), identities(None)
    assert random_a != random_b
    spec, pmbr, primary, entries, backup = metadata(first)
    assert spec["blocks"] * 512 == DISK_SIZE
    assert len(entries) == 16384 and primary[56:72] == backup[56:72]
    assert pmbr[510:512] == b"\x55\xaa"


def test_gpt_checker_rejects_crc_and_partition_mutations():
    spec, pmbr, primary, entries, backup = metadata(identities("mutations"))
    bad_header = bytearray(primary)
    bad_header[16] ^= 1
    with pytest.raises(ValueError, match="CRC"):
        validate_gpt(bad_header, backup, entries, entries, pmbr, DISK_SIZE, 512)

    for altered_start in (spec["partitions"][1][0], spec["partitions"][2][0] + 1):
        broken_entries = bytearray(entries)
        # Tạo overlap và sai alignment, tính lại CRC để validator kiểm tra semantic.
        struct.pack_into("<Q", broken_entries, 2 * 128 + 32, altered_start)
        changed_headers = []
        for header in (primary, backup):
            changed = bytearray(header)
            struct.pack_into("<I", changed, 88, zlib.crc32(broken_entries))
            struct.pack_into("<I", changed, 16, 0)
            struct.pack_into("<I", changed, 16, zlib.crc32(changed[:92]))
            changed_headers.append(changed)
        with pytest.raises(ValueError, match="partition bounds/overlap"):
            validate_gpt(changed_headers[0], changed_headers[1], broken_entries,
                         broken_entries, pmbr, DISK_SIZE, 512)


def test_image_destination_guards(tmp_path):
    with pytest.raises(ValueError, match="chỉ được tạo"):
        create_image(Path("/dev/null"), None, None)
    existing = tmp_path / "existing.img"
    existing.write_bytes(b"protected")
    link = tmp_path / "link.img"
    link.symlink_to(existing)
    with pytest.raises(FileExistsError):
        create_image(link, None, None)
    assert existing.read_bytes() == b"protected"


@pytest.mark.usefixtures("m07_image")
def test_generated_image_is_sparse_and_only_esp_is_populated(m07_image):
    image, report, bios, uefi = m07_image
    assert image.stat().st_size == DISK_SIZE
    assert image.stat().st_blocks * 512 < 600 * 1024**2
    assert report["system_partitions_formatted"] is False
    with image.open("rb") as disk:
        for offset in (259 * 1024**2, 2307 * 1024**2, 4355 * 1024**2):
            disk.seek(offset)
            assert disk.read(512) == bytes(512)
    assert report["kernel_sha256"] == file_hash(bios / "kernel.elf")
    assert report["efi_sha256"] == file_hash(uefi / "BOOTX64.EFI")


def test_generator_refuses_overwrite(m07_image):
    image, _, bios, uefi = m07_image
    before = protected_hashes(image, bios, uefi)
    with pytest.raises(FileExistsError):
        create_image(image, bios, uefi, seed="m07-tests-v1")
    assert protected_hashes(image, bios, uefi) == before


def test_checker_rejects_truncated_image(tmp_path):
    small = tmp_path / "truncated.img"
    small.write_bytes(bytes(4096))
    with pytest.raises(ValueError, match="kích thước"):
        check_image(small)


def parse_lbp(serial):
    line = next(item for item in serial if item.startswith("M07:LBP "))
    fields = line.split()
    assert len(fields) == 8, line
    return {"major": int(fields[1], 16), "minor": int(fields[2], 16),
            "source": int(fields[3], 16), "slot": int(fields[4], 16),
            "required": int(fields[5], 16), "provided": int(fields[6], 16),
            "records": int(fields[7], 16)}


@pytest.mark.qemu
def test_same_image_and_kernel_boot_through_bios_and_uefi(m07_image):
    image, build_report, bios, uefi = m07_image
    before = protected_hashes(image, bios, uefi)
    bios_result = run(Config(profile="bios", purpose="bios-stage2", image=image,
                             success_marker="M05:PASS", panic_marker="M05:FAIL",
                             timeout=30, output_root=ROOT / "out/m07/bios"))
    uefi_result = run(Config(profile="uefi", purpose="uefi-loader", image=image,
                             success_marker="M06:PASS", panic_marker="M06:FAIL",
                             timeout=30, output_root=ROOT / "out/m07/uefi"))
    bios_serial = (Path(bios_result.directory) / "serial.log").read_text(errors="replace").splitlines()
    uefi_serial = (Path(uefi_result.directory) / "serial.log").read_text(errors="replace").splitlines()
    bios_lbp, uefi_lbp = parse_lbp(bios_serial), parse_lbp(uefi_serial)
    assert bios_result.status == uefi_result.status == "PASS"
    assert bios_result.returncode == uefi_result.returncode == 0
    assert bios_serial[-3] == "M05:HANDOFF" and bios_serial[-1] == "M05:PASS"
    assert uefi_serial[-1] == "M06:PASS"
    assert bios_lbp["major"] == uefi_lbp["major"] == 1
    assert bios_lbp["minor"] == uefi_lbp["minor"] == 0
    assert (bios_lbp["source"], uefi_lbp["source"]) == (1, 2)
    assert bios_lbp["slot"] == uefi_lbp["slot"] == 1
    assert bios_lbp["required"] == uefi_lbp["required"]
    assert bios_lbp["provided"] & 4 and uefi_lbp["provided"] & 4
    assert bios_lbp["records"] >= 5 and uefi_lbp["records"] >= 5
    after = protected_hashes(image, bios, uefi)
    assert before == after
    assert build_report["kernel_sha256"] == before["kernel"]
    assert not bios_result.cleanup_forced and not bios_result.cleanup_error
    assert not uefi_result.cleanup_forced and not uefi_result.cleanup_error
    assert any("snapshot=on" in item for item in bios_result.command)
    assert any("snapshot=on" in item for item in uefi_result.command)
    assert any("readonly=on" in item for item in uefi_result.command)
    assert str(Path(uefi_result.directory) / "OVMF_VARS.fd") in " ".join(uefi_result.command)

    # Cả firmware vẫn nạp được loader; hai loader phải từ chối cùng ELF hỏng.
    broken = image.parent / "bad-esp.img"
    sparse_clone(image, broken)
    with broken.open("r+b") as disk:
        disk.seek(kernel_payload_offset(broken))
        disk.write(b"\0")  # Hỏng ELF magic, giữ nguyên FAT directory/boot path.
    bad_bios = run(Config(profile="bios", purpose="bios-stage2", image=broken,
                          success_marker="M05:PASS", panic_marker="M05:FAIL",
                          timeout=30, output_root=ROOT / "out/m07/negative-bios"))
    bad_uefi = run(Config(profile="uefi", purpose="uefi-loader", image=broken,
                          success_marker="M06:PASS", panic_marker="M06:FAIL",
                          timeout=30, output_root=ROOT / "out/m07/negative-uefi"))
    bad_bios_serial = (Path(bad_bios.directory) / "serial.log").read_text(errors="replace").splitlines()
    bad_uefi_serial = (Path(bad_uefi.directory) / "serial.log").read_text(errors="replace").splitlines()
    assert bad_bios.status == bad_uefi.status == "PANIC"
    assert bad_bios_serial[bad_bios_serial.index("M05:FAIL") + 1] == "ELF"
    assert bad_uefi_serial[bad_uefi_serial.index("M06:FAIL") + 1] == "ELF"
    assert not bad_bios.cleanup_forced and not bad_bios.cleanup_error
    assert not bad_uefi.cleanup_forced and not bad_uefi.cleanup_error
    assert before == protected_hashes(image, bios, uefi)
    report = {"status": "PASS", "same_image": True, "same_kernel_sha256": before["kernel"],
              "image": build_report,
              "bios": {"result": bios_result.__dict__, "lbp": bios_lbp, "serial": bios_serial},
              "uefi": {"result": uefi_result.__dict__, "lbp": uefi_lbp, "serial": uefi_serial},
              "negative_bad_elf": {"bios": bad_bios_serial, "uefi": bad_uefi_serial,
                                   "statuses": [bad_bios.status, bad_uefi.status]},
              "protected_hashes_before": before, "protected_hashes_after": after}
    target = ROOT / "out/m07/parity.json"
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
