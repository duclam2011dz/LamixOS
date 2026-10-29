# SPDX-License-Identifier: GPL-3.0-only
"""Host PMM/provenance checks và kernel PMM guest BIOS/UEFI M09."""
import ctypes as C
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys

import pytest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tests/boot"))
from fixtures import bootinfo, offsets

sys.path.insert(0, str(ROOT))
from scripts.m07_image import DISK_SIZE, check_image, create_image
from scripts.qemu_harness import Config, run

OK, INVALID_ARGUMENT, INVALID_MAP, STORAGE_TOO_SMALL, OUT_OF_MEMORY, OUT_OF_RANGE, NOT_ALLOCATED = range(7)
PMM_PAGE_SIZE = 4096
PMM_LIMIT = 1 << 32
PMM_FRAME_COUNT = PMM_LIMIT // PMM_PAGE_SIZE
PMM_STATE_BYTES = PMM_FRAME_COUNT // 4


class View(C.Structure):
    _fields_ = [("bytes", C.c_void_p), ("size", C.c_uint64), ("count", C.c_uint32)]


class Pmm(C.Structure):
    _fields_ = [("states", C.c_void_p), ("free_frames", C.c_uint32),
                ("allocated_frames", C.c_uint32), ("next_hint", C.c_uint32),
                ("initialized", C.c_uint32)]


@pytest.fixture(scope="session")
def pmm_lib(tmp_path_factory):
    output = tmp_path_factory.mktemp("m09-pmm") / "pmm.so"
    subprocess.run(["gcc", "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
                    "-shared", "-fPIC", "-I", str(ROOT / "include"), "-I",
                    str(ROOT / "kernel"), str(ROOT / "kernel/pmm.c"),
                    str(ROOT / "lib/boot/contract.c"), str(ROOT / "lib/boot/memory.c"),
                    "-o", str(output)], check=True, timeout=120)
    library = C.CDLL(str(output))
    pointer, u64 = C.c_void_p, C.c_uint64
    library.lbp_validate.argtypes = [pointer, u64, pointer, u64, C.POINTER(View)]
    library.pmm_init.argtypes = [C.POINTER(Pmm), C.POINTER(View), pointer, u64]
    library.pmm_alloc_frame.argtypes = [C.POINTER(Pmm), C.POINTER(u64)]
    library.pmm_free_frame.argtypes = [C.POINTER(Pmm), u64]
    return library


def validated(library, blob):
    data = C.create_string_buffer(bytes(blob))
    view = View()
    assert library.lbp_validate(data, len(blob), None, 0, C.byref(view)) == OK
    return data, view


def new_manager(library, blob, storage_size=PMM_STATE_BYTES):
    data, view = validated(library, blob)
    storage = C.create_string_buffer(max(storage_size, 1))
    manager = Pmm()
    status = library.pmm_init(C.byref(manager), C.byref(view), storage,
                              storage_size)
    return status, manager, storage, data, view


def test_kernel_m09_build_has_provenance_and_4g_bitmap(tmp_path):
    build = ROOT / "build/m09-host-kernel"
    subprocess.run([sys.executable, str(ROOT / "tests/m09/build_kernel.py"),
                    "--build", str(build)], check=True, timeout=180,
                   stdout=subprocess.DEVNULL)
    report = json.loads((build / "build.json").read_text())
    assert report["status"] == "PASS" and report["mode"] == "m09-kernel"
    assert report["hashes"]["kernel.elf"] == hashlib.sha256(
        (build / "kernel.elf").read_bytes()).hexdigest()
    assert report["source_hashes"]["kernel/pmm.c"]
    assert report["source_hashes"]["kernel/pmm.h"]
    inspection = (build / "kernel.inspection.txt").read_text()
    assert "There are no relocations" in inspection or "Relocation section" not in inspection


def test_pmm_allocates_only_usable_frames_below_4g(pmm_lib):
    status, manager, storage, data, view = new_manager(pmm_lib, bootinfo())
    assert status == OK and manager.free_frames == 16
    frames = []
    for _ in range(16):
        physical = C.c_uint64(0xfeedface)
        assert pmm_lib.pmm_alloc_frame(C.byref(manager), C.byref(physical)) == OK
        frames.append(physical.value)
    assert frames == [0x200000 + i * PMM_PAGE_SIZE for i in range(16)]
    assert all(frame < PMM_LIMIT for frame in frames)
    untouched = C.c_uint64(0xfeedface)
    assert pmm_lib.pmm_alloc_frame(C.byref(manager), C.byref(untouched)) == OUT_OF_MEMORY
    assert untouched.value == 0xfeedface
    assert manager.free_frames == 0 and manager.allocated_frames == 16
    assert pmm_lib.pmm_free_frame(C.byref(manager), 0x130000) == NOT_ALLOCATED
    assert pmm_lib.pmm_free_frame(C.byref(manager), 0x100000000) == OUT_OF_RANGE
    assert pmm_lib.pmm_free_frame(C.byref(manager), frames[0] + 1) == INVALID_ARGUMENT
    for frame in frames:
        assert pmm_lib.pmm_free_frame(C.byref(manager), frame) == OK
    assert manager.free_frames == 16 and manager.allocated_frames == 0
    assert pmm_lib.pmm_free_frame(C.byref(manager), frames[0]) == NOT_ALLOCATED


def test_pmm_supports_multiple_extents_and_never_allocates_holes(pmm_lib):
    maps = [(0x100000, 0x40000, 2, 0, 0), (0x200000, 0x2000, 1, 0, 0),
            (0x300000, 0x1000, 1, 0, 0), (0x100000000, 0x2000, 1, 0, 0)]
    status, manager, storage, data, view = new_manager(pmm_lib, bootinfo(memory=maps))
    assert status == OK and manager.free_frames == 3
    actual = []
    for _ in range(3):
        physical = C.c_uint64()
        assert pmm_lib.pmm_alloc_frame(C.byref(manager), C.byref(physical)) == OK
        actual.append(physical.value)
    assert actual == [0x200000, 0x201000, 0x300000]
    physical = C.c_uint64()
    assert pmm_lib.pmm_alloc_frame(C.byref(manager), C.byref(physical)) == OUT_OF_MEMORY


def test_pmm_rejects_small_storage_and_invalidated_map_without_partial_state(pmm_lib):
    blob = bootinfo()
    data, view = validated(pmm_lib, blob)
    manager = Pmm()
    storage = C.create_string_buffer(PMM_STATE_BYTES)
    C.memset(storage, 0xA5, PMM_STATE_BYTES)
    assert pmm_lib.pmm_init(C.byref(manager), C.byref(view), storage,
                            PMM_STATE_BYTES - 1) == STORAGE_TOO_SMALL
    assert bytes(storage.raw) == bytes([0xA5]) * PMM_STATE_BYTES
    memory_record = offsets(blob)[1]
    C.c_uint32.from_buffer(data, memory_record + 20).value = 0
    assert pmm_lib.pmm_init(C.byref(manager), C.byref(view), storage,
                            PMM_STATE_BYTES) == INVALID_MAP
    assert bytes(storage.raw) == bytes([0xA5]) * PMM_STATE_BYTES


def test_lbp_validator_rejects_malformed_memory_map_before_pmm(pmm_lib):
    blob = bootinfo()
    C.c_uint64.from_buffer(blob, offsets(blob)[1] + 32 + 8).value = (1 << 64) - 1
    data = C.create_string_buffer(bytes(blob))
    view = View()
    assert pmm_lib.lbp_validate(data, len(blob), None, 0, C.byref(view)) != OK


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


@pytest.fixture(scope="session")
def m09_image(tmp_path_factory):
    bios, uefi = artifacts()
    kernel_build = ROOT / "build/m09-guest"
    subprocess.run([sys.executable, str(ROOT / "tests/m09/build_kernel.py"),
                    "--build", str(kernel_build)], check=True, timeout=180,
                   stdout=subprocess.DEVNULL)
    image = tmp_path_factory.mktemp("m09-guest") / "lamix.img"
    report = create_image(image, bios, uefi, kernel_build=kernel_build,
                          seed="m09-pmm-guest")
    checked = check_image(image, bios, uefi, kernel_build=kernel_build)
    assert report["kernel_sha256"] == checked["kernel_sha256"]
    return image, report, bios, uefi, kernel_build


def protected_hashes(image, bios, uefi, kernel_build):
    def digest(path):
        return hashlib.sha256(Path(path).read_bytes()).hexdigest()
    extents = ((0, 512), (512, 512 + 512 + 16384),
               (1024 * 1024, 3 * 1024 * 1024),
               (3 * 1024**2, 3 * 1024**2 + 256 * 1024**2),
               (DISK_SIZE - 33 * 512, DISK_SIZE))
    image_hashes = []
    with Path(image).open("rb") as stream:
        for start, end in extents:
            stream.seek(start)
            hasher, remaining = hashlib.sha256(), end - start
            while remaining:
                block = stream.read(min(1024 * 1024, remaining))
                assert block
                hasher.update(block)
                remaining -= len(block)
            image_hashes.append(hasher.hexdigest())
    return {"image_extents": image_hashes,
            "seabios": digest("/usr/share/seabios/bios.bin"),
            "ovmf_code": digest("/usr/share/OVMF/OVMF_CODE.fd"),
            "ovmf_vars_template": digest("/usr/share/OVMF/OVMF_VARS.fd"),
            "stage1": digest(bios / "stage1.bin"),
            "stage2": digest(bios / "stage2.bin"),
            "efi": digest(uefi / "BOOTX64.EFI"),
            "kernel": digest(kernel_build / "kernel.elf")}


@pytest.mark.qemu
@pytest.mark.parametrize("profile,purpose", [("bios", "bios-stage2"),
                                               ("uefi", "uefi-loader")])
def test_pmm_guest_boots_same_kernel_bios_and_uefi(m09_image, profile, purpose):
    image, build_report, bios, uefi, kernel_build = m09_image
    before = protected_hashes(image, bios, uefi, kernel_build)
    result = run(Config(profile=profile, purpose=purpose, image=image,
                        success_marker="M09:PASS", panic_marker="M09:FAIL",
                        timeout=30, output_root=ROOT / "out/m09"))
    after = protected_hashes(image, bios, uefi, kernel_build)
    serial = (Path(result.directory) / "serial.log").read_text(errors="replace")
    assert result.status == "PASS", result.detail
    assert "M08:PASS\n" in serial and "M09:PASS\n" in serial
    pmm_line = next(line for line in serial.splitlines() if line.startswith("M09:PMM "))
    fields = dict(part.split("=", 1) for part in pmm_line.split()[1:])
    assert fields["frames"] == "8" and int(fields["free"], 16) > 8
    assert build_report["kernel_sha256"] == before["kernel"]
    assert after == before
    assert not result.cleanup_forced and result.cleanup_error is None
    report = {"status": result.status, "profile": profile, "purpose": purpose,
              "kernel_sha256": build_report["kernel_sha256"],
              "free_frames": fields["free"], "qemu": result.__dict__,
              "serial": serial, "protected_hashes_before": before,
              "protected_hashes_after": after}
    target = ROOT / "out/m09/results" / f"{profile}.json"
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n",
                      encoding="utf-8")
