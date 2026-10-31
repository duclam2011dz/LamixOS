#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Tạo và kiểm tra image GPT M07; chỉ tạo regular file mới trong build/out/tmp."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile
import uuid
import zlib

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tests/boot"))
from gpt_model import layout, ROLES, TYPES, validate

DISK_SIZE = 8 * 1024**3
ESP_SIZE = 256 * 1024**2
SECTOR = 512
SEED_NAMESPACE = uuid.UUID("f42d09e5-4fcb-5dd3-8a7f-594a179955d5")
ESP_OFFSET = 3 * 1024**2
ESP_PATHS = ("EFI/BOOT/BOOTX64.EFI", "EFI/LAMIX/A/KERNEL.ELF")


def digest(path):
    h = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def tool(name):
    result = shutil.which(name)
    if not result:
        raise RuntimeError(f"thiếu công cụ {name}; cài dependency theo docs/TOOLCHAIN.md")
    return result


def tool_version(name, args):
    output = subprocess.check_output([tool(name), *args], text=True,
                                     stderr=subprocess.STDOUT, timeout=15)
    if name == "mkfs.fat":
        return next((line.strip() for line in output.splitlines()
                     if line.startswith("mkfs.fat ")), output.splitlines()[-1])
    return output.splitlines()[0].strip()


def checked_artifact(path, maximum=None):
    path = Path(path)
    if path.is_symlink() or not path.is_file():
        raise ValueError(f"artifact không phải regular file: {path}")
    path = path.resolve(strict=True)
    size = path.stat().st_size
    if not size or (maximum is not None and size > maximum):
        raise ValueError(f"kích thước artifact không hợp lệ: {path}")
    return path


def verify_build_report(directory, required_hashes):
    report_path = Path(directory) / "build.json"
    report = json.loads(report_path.read_text(encoding="utf-8"))
    if report.get("status") != "PASS":
        raise ValueError(f"build report chưa PASS: {report_path}")
    for name, expected in report.get("source_hashes", {}).items():
        source = ROOT / name
        if not source.is_file() or digest(source) != expected:
            raise ValueError(f"source đã đổi so với build: {name}")
    for name in required_hashes:
        expected = report.get("hashes", {}).get(name)
        artifact = Path(directory) / name
        if not expected or digest(artifact) != expected:
            raise ValueError(f"artifact thiếu hoặc khác build report: {name}")
    return report


def identities(seed):
    labels = ["disk", "bios-boot", "esp", "system-a", "system-b", "user-data"]
    if seed is None:
        values = [uuid.uuid4() for _ in labels]
    else:
        if not seed or len(seed.encode("utf-8")) > 1024:
            raise ValueError("seed phải là UTF-8 không rỗng, tối đa 1024 byte")
        values = [uuid.uuid5(SEED_NAMESPACE, seed + "/" + label) for label in labels]
    if any(value.int == 0 for value in values) or len(set(values)) != len(values):
        raise ValueError("GUID identity không zero và phải duy nhất")
    return values


def metadata(identity):
    spec = layout(DISK_SIZE, SECTOR)
    disk_guid, *part_guids = identity
    entries = bytearray(16384)
    for index, ((start, end), name, typ, guid) in enumerate(
            zip(spec["partitions"], ROLES, TYPES, part_guids)):
        encoded_name = name.encode("utf-16le").ljust(72, b"\0")
        struct.pack_into("<16s16sQQQ72s", entries, index * 128,
                         typ.bytes_le, guid.bytes_le, start, end, 0, encoded_name)

    def header(current, other, array):
        data = bytearray(SECTOR)
        struct.pack_into("<8sIIIIQQQQ16sQIII", data, 0, b"EFI PART", 0x10000,
                         92, 0, 0, current, other, spec["first"], spec["last"],
                         disk_guid.bytes_le, array, 128, 128, zlib.crc32(entries))
        struct.pack_into("<I", data, 16, zlib.crc32(data[:92]))
        return data

    pmbr = bytearray(SECTOR)
    pmbr[446:450] = b"\0\0\2\0"
    pmbr[450:454] = b"\xee\xff\xff\xff"
    struct.pack_into("<II", pmbr, 454, 1, min(spec["blocks"] - 1, 0xffffffff))
    pmbr[510:512] = b"\x55\xaa"
    primary = header(1, spec["blocks"] - 1, 2)
    backup = header(spec["blocks"] - 1, 1, spec["backup_array"])
    validate(primary, backup, entries, entries, pmbr, DISK_SIZE, SECTOR)
    return spec, pmbr, primary, entries, backup


def within_allowed(path):
    resolved_parent = Path(path).parent.resolve(strict=False)
    roots = (ROOT / "build", ROOT / "out", Path("/tmp").resolve())
    return any(resolved_parent == root or resolved_parent.is_relative_to(root)
               for root in roots)


def make_esp(efi, kernel, directory):
    image = directory / "esp.img"
    with image.open("xb") as stream:
        stream.truncate(ESP_SIZE)
    subprocess.run([tool("mkfs.fat"), "-F", "32", "-n", "LAMIXESP", str(image)],
                   check=True, timeout=60, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    for folder in ("::EFI", "::EFI/BOOT", "::EFI/LAMIX", "::EFI/LAMIX/A"):
        subprocess.run([tool("mmd"), "-i", str(image), folder], check=True, timeout=30,
                       stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    for source, target in ((efi, "::EFI/BOOT/BOOTX64.EFI"),
                           (kernel, "::EFI/LAMIX/A/KERNEL.ELF")):
        subprocess.run([tool("mcopy"), "-i", str(image), str(source), target],
                       check=True, timeout=60, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    # mkfs.fat voit un volume temporaire offset 0; l'API FAT M05 vérifie le LBA ESP.
    # Répercuter HiddenSectors dans les deux BPB FAT32 sans toucher aux clusters.
    with image.open("r+b") as stream:
        for sector in (0, 6):
            stream.seek(sector * SECTOR + 28)
            stream.write(struct.pack("<I", ESP_OFFSET // SECTOR))
    return image


def create_image(destination, bios_build, uefi_build, kernel=None, seed=None, kernel_build=None):
    destination = Path(destination)
    if not within_allowed(destination):
        raise ValueError("image chỉ được tạo dưới build/, out/ hoặc /tmp")
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination = destination.parent.resolve(strict=True) / destination.name
    if destination.exists() or destination.is_symlink():
        raise FileExistsError(destination)
    bios_build = Path(bios_build).resolve(strict=True)
    uefi_build = Path(uefi_build).resolve(strict=True)
    bios_report = verify_build_report(bios_build, ("stage1.bin", "stage2.bin", "kernel.elf"))
    uefi_report = verify_build_report(uefi_build, ("BOOTX64.EFI",))
    if kernel is not None and kernel_build is not None:
        raise ValueError("chỉ chọn --kernel hoặc --kernel-build")
    kernel_report = None
    if kernel_build is not None:
        kernel_build = Path(kernel_build).resolve(strict=True)
        kernel_report = verify_build_report(kernel_build, ("kernel.elf",))
        if kernel_report.get("mode") not in ("m08-kernel", "m09-kernel", "m10-kernel", "m11-kernel", "m12-kernel", "m13-kernel", "m14-kernel", "m15-kernel", "m16-kernel"):
            raise ValueError("kernel build report không thuộc profile kernel M08–M16")
        kernel = checked_artifact(kernel_build / "kernel.elf", 16 * 1024**2)
    else:
        kernel = checked_artifact(kernel or bios_build / "kernel.elf", 16 * 1024**2)
    stage1 = checked_artifact(bios_build / "stage1.bin", 440)
    stage2 = checked_artifact(bios_build / "stage2.bin", 2 * 1024**2)
    efi = checked_artifact(uefi_build / "BOOTX64.EFI", 16 * 1024**2)
    if stage1.stat().st_size != 440 or stage2.stat().st_size < 32768:
        raise ValueError("BIOS artifact M05 không đúng kích thước đã nghiệm thu")
    if kernel_build is None and kernel != (bios_build / "kernel.elf").resolve(strict=True):
        raise ValueError("M07 yêu cầu kernel.elf chung trực tiếp từ build M05")
    identities_used = identities(seed)
    spec, pmbr, primary, entries, backup = metadata(identities_used)
    pmbr[:440] = stage1.read_bytes()
    bios_start = spec["partitions"][0][0] * SECTOR
    if len(stage2.read_bytes()) > (spec["partitions"][0][1] - spec["partitions"][0][0] + 1) * SECTOR:
        raise ValueError("Stage2 vượt BIOS Boot Partition")
    destination.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="m07-esp-", dir=destination.parent) as temporary:
        esp = make_esp(efi, kernel, Path(temporary))
        destination_created = False
        try:
            with destination.open("xb") as disk, esp.open("rb") as source:
                destination_created = True
                disk.truncate(DISK_SIZE)
                def write(offset, data):
                    disk.seek(offset)
                    disk.write(data)
                write(0, pmbr)
                write(SECTOR, primary)
                write(2 * SECTOR, entries)
                write(spec["backup_array"] * SECTOR, entries)
                write((spec["blocks"] - 1) * SECTOR, backup)
                write(bios_start, stage2.read_bytes())
                disk.seek(ESP_OFFSET)
                shutil.copyfileobj(source, disk, 1024 * 1024)
                disk.flush()
                os.fsync(disk.fileno())
        except BaseException:
            if destination_created:
                destination.unlink(missing_ok=True)
            raise
    report = {
        "status": "PASS", "mode": "m07-image", "image_bytes": DISK_SIZE,
        "allocated_bytes": destination.stat().st_blocks * 512,
        "seed": seed, "disk_guid": str(identities_used[0]),
        "partition_guids": [str(value) for value in identities_used[1:]],
        "kernel_sha256": digest(kernel), "stage1_sha256": digest(stage1),
        "stage2_sha256": digest(stage2), "efi_sha256": digest(efi),
        "tool_versions": {"mkfs.fat": tool_version("mkfs.fat", ["--help"]),
                          "mtools": tool_version("mcopy", ["-V"]),
                          "qemu": subprocess.check_output([tool("qemu-system-x86_64"), "--version"],
                                                          text=True, stderr=subprocess.STDOUT,
                                                          timeout=15).splitlines()[0]},
        "artifact_tool_versions": {"m05": bios_report.get("tool_versions", {}),
                                   "m06": uefi_report.get("tool_versions", {}),
                                   "kernel": kernel_report.get("tool_versions", {}) if kernel_report else {}},
        "paths": list(ESP_PATHS), "system_partitions_formatted": False,
    }
    return report


def check_image(path, bios_build=None, uefi_build=None, kernel=None, report=None, kernel_build=None):
    path = checked_artifact(path)
    if path.stat().st_size != DISK_SIZE:
        raise ValueError("kích thước image không đúng profile 8 GiB")
    spec = layout(DISK_SIZE, SECTOR)
    with path.open("rb") as disk:
        def read(offset, size):
            disk.seek(offset)
            data = disk.read(size)
            if len(data) != size:
                raise ValueError("image bị cắt ngắn")
            return data
        pmbr = read(0, SECTOR)
        stage1 = pmbr[:440]
        primary = read(SECTOR, SECTOR)
        entries = read(2 * SECTOR, 16384)
        backup_entries = read(spec["backup_array"] * SECTOR, 16384)
        backup = read((spec["blocks"] - 1) * SECTOR, SECTOR)
        validated = validate(primary, backup, entries, backup_entries, pmbr,
                              DISK_SIZE, SECTOR)
        esp_start, esp_end = validated["partitions"][1]
        esp_bpb = read(esp_start * SECTOR, SECTOR)
        bps, spc = struct.unpack_from("<HB", esp_bpb, 11)
        reserved, fats = struct.unpack_from("<HB", esp_bpb, 14)
        total16 = struct.unpack_from("<H", esp_bpb, 19)[0]
        fat16 = struct.unpack_from("<H", esp_bpb, 22)[0]
        hidden, total32, fat32, flags, _fs_version, root = struct.unpack_from("<IIIHHI", esp_bpb, 28)
        backup_sector = struct.unpack_from("<H", esp_bpb, 50)[0]
        esp_sectors = esp_end - esp_start + 1
        data_start = reserved + fats * fat32
        clusters = (total32 - data_start) // spc if spc else 0
        if (esp_bpb[510:512] != b"\x55\xaa" or bps != SECTOR or not spc or
                spc > 64 or spc & (spc - 1) or not reserved or
                fats not in (1, 2) or total16 or fat16 or not fat32 or
                hidden != esp_start or total32 != esp_sectors or flags & 0xff70 or
                _fs_version != 0 or backup_sector != 6 or
                clusters < 65525 or (clusters + 2) * 4 > fat32 * SECTOR or
                not 2 <= root < clusters + 2):
            raise ValueError("ESP BPB không đúng profile FAT32 Lamix")
        backup_bpb = read((esp_start + 6) * SECTOR, SECTOR)
        if backup_bpb[11:90] != esp_bpb[11:90] or backup_bpb[510:512] != b"\x55\xaa":
            raise ValueError("backup BPB FAT32 khác BPB chính")
        if stage1 == bytes(440):
            raise ValueError("Stage1 trong PMBR bị rỗng")
        if read(spec["partitions"][0][0] * SECTOR + 32704, 7) != b"LAMIXS2":
            raise ValueError("Stage2 bootstrap không nằm ở đầu BIOS Boot Partition")
    offset_arg = f"{path}@@{ESP_OFFSET}"
    listing = subprocess.run([tool("mdir"), "-i", offset_arg, "-b", "::/"],
                             check=True, timeout=30, text=True, capture_output=True).stdout
    for path_name in ESP_PATHS:
        target = "::/" + path_name
        subprocess.run([tool("mdir"), "-i", offset_arg, target], check=True,
                       timeout=30, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    forbidden = subprocess.run([tool("mdir"), "-i", offset_arg, "::/EFI/LAMIX/B"],
                               timeout=30, text=True, capture_output=True)
    if forbidden.returncode == 0:
        raise ValueError("ESP không được chứa payload slot B trong M07")
    extracted = {}
    if kernel is not None and kernel_build is not None:
        raise ValueError("chỉ chọn --kernel hoặc --kernel-build")
    kernel_report = None
    if kernel_build is not None:
        kernel_build = Path(kernel_build).resolve(strict=True)
        kernel_report = verify_build_report(kernel_build, ("kernel.elf",))
        if kernel_report.get("mode") not in ("m08-kernel", "m09-kernel", "m10-kernel", "m11-kernel", "m12-kernel", "m13-kernel", "m14-kernel", "m15-kernel", "m16-kernel"):
            raise ValueError("kernel build report không thuộc profile kernel M08–M16")
        kernel = checked_artifact(kernel_build / "kernel.elf", 16 * 1024**2)
    if bios_build or uefi_build or kernel or kernel_build:
        if not bios_build or not uefi_build:
            raise ValueError("checker cần cả --bios-build và --uefi-build")
        bios_build, uefi_build = Path(bios_build), Path(uefi_build)
        expected = {
            "EFI/BOOT/BOOTX64.EFI": checked_artifact(uefi_build / "BOOTX64.EFI"),
            "EFI/LAMIX/A/KERNEL.ELF": checked_artifact(kernel or bios_build / "kernel.elf"),
        }
        with tempfile.TemporaryDirectory(prefix="m07-check-") as temporary:
            for name, source in expected.items():
                target = Path(temporary) / ("payload-" + str(len(extracted)))
                subprocess.run([tool("mcopy"), "-i", offset_arg, "::/" + name,
                                str(target)], check=True, timeout=60,
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE)
                if target.stat().st_size != source.stat().st_size or digest(target) != digest(source):
                    raise ValueError(f"ESP payload khác artifact: {name}")
                extracted[name] = {"bytes": target.stat().st_size, "sha256": digest(target)}
        if bios_build:
            stage1_expected = checked_artifact(Path(bios_build) / "stage1.bin", 440)
            stage2_expected = checked_artifact(Path(bios_build) / "stage2.bin", 2 * 1024**2)
            with path.open("rb") as disk:
                if disk.read(440) != stage1_expected.read_bytes():
                    raise ValueError("Stage1 trong PMBR khác artifact")
                disk.seek(spec["partitions"][0][0] * SECTOR)
                if disk.read(stage2_expected.stat().st_size) != stage2_expected.read_bytes():
                    raise ValueError("Stage2 khác artifact M05")
    actual = {
        "status": "PASS", "mode": "m07-image-check", "image_bytes": path.stat().st_size,
        "allocated_bytes": path.stat().st_blocks * 512,
        "disk_guid": str(uuid.UUID(bytes_le=primary[56:72])),
        "kernel_sha256": extracted.get("EFI/LAMIX/A/KERNEL.ELF", {}).get("sha256"),
        "efi_sha256": extracted.get("EFI/BOOT/BOOTX64.EFI", {}).get("sha256"),
        "payloads": extracted, "paths": list(ESP_PATHS),
        "esp_root_listing": listing.splitlines(), "partitions": validated["partitions"],
    }
    if report:
        for key in ("image_bytes", "disk_guid", "kernel_sha256", "paths"):
            if report.get(key) is not None and actual.get(key) is not None:
                if report[key] != actual[key]:
                    raise ValueError(f"report/image mismatch: {key}")
    return actual


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)
    create = sub.add_parser("create")
    create.add_argument("--image", type=Path, required=True)
    create.add_argument("--bios-build", type=Path, required=True)
    create.add_argument("--uefi-build", type=Path, required=True)
    create.add_argument("--kernel", type=Path)
    create.add_argument("--kernel-build", type=Path)
    create.add_argument("--seed")
    check = sub.add_parser("check")
    check.add_argument("--image", type=Path, required=True)
    check.add_argument("--bios-build", type=Path, required=True)
    check.add_argument("--uefi-build", type=Path, required=True)
    check.add_argument("--kernel", type=Path)
    check.add_argument("--kernel-build", type=Path)
    args = parser.parse_args()
    if args.command == "create":
        report = create_image(args.image, args.bios_build, args.uefi_build,
                              args.kernel, args.seed, args.kernel_build)
    else:
        report = check_image(args.image, args.bios_build, args.uefi_build, args.kernel,
                             kernel_build=args.kernel_build)
    print(json.dumps(report, ensure_ascii=False, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
