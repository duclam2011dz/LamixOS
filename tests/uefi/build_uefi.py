#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Build và kiểm tra EFI PE/COFF trong build/, không cài dependencies."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import struct
import subprocess

ROOT = Path(__file__).resolve().parents[2]
SOURCES = [
    "boot/uefi/entry.asm", "boot/uefi/trampoline.asm", "boot/uefi/efi.h",
    "boot/uefi/loader.c", "boot/uefi/runtime.c", "boot/uefi/policy.h",
    "include/lamix/boot/lbp.h",
    "include/lamix/boot/contract.h", "include/lamix/boot/loader.h",
    "lib/boot/contract.c", "lib/boot/memory.c", "lib/boot/loader.c",
]


def require_tool(name):
    found = shutil.which(name)
    if not found:
        raise RuntimeError(f"thiếu công cụ {name}; chưa cài dependencies")
    return found


def inspect(path):
    data = path.read_bytes()
    if len(data) < 0x100 or data[:2] != b"MZ":
        raise ValueError("PE thiếu DOS header")
    pe = struct.unpack_from("<I", data, 0x3c)[0]
    if data[pe:pe + 4] != b"PE\0\0":
        raise ValueError("PE signature sai")
    machine, sections, _, _, _, opt_size, _ = struct.unpack_from("<HHIIIHH", data, pe + 4)
    opt = pe + 24
    magic = struct.unpack_from("<H", data, opt)[0]
    entry = struct.unpack_from("<I", data, opt + 16)[0]
    image_base = struct.unpack_from("<Q", data, opt + 24)[0]
    subsystem = struct.unpack_from("<H", data, opt + 68)[0]
    dirs = opt + 112
    imports = struct.unpack_from("<II", data, dirs + 8)[0]
    reloc_rva, reloc_size = struct.unpack_from("<II", data, dirs + 5 * 8)
    if (machine, magic, subsystem) != (0x8664, 0x20b, 10):
        raise ValueError("cần PE32+ x64 EFI application")
    if not entry or imports or not reloc_rva or not reloc_size:
        raise ValueError("entry/imports/relocations PE không hợp lệ")
    return {"machine": machine, "sections": sections, "optional_header_size": opt_size,
            "magic": magic, "entry_rva": entry, "image_base": image_base,
            "subsystem": subsystem, "imports_rva": imports,
            "relocations": {"rva": reloc_rva, "size": reloc_size}, "bytes": len(data)}


def build(output):
    output = output.resolve()
    if not output.is_relative_to(ROOT / "build"):
        raise ValueError("output phải nằm dưới build/")
    output.mkdir(parents=True, exist_ok=True)
    clang, linker, nasm = map(require_tool, ("clang", "lld-link", "nasm"))
    commands = []

    def run(args):
        args = list(map(str, args))
        commands.append(args)
        subprocess.run(args, cwd=ROOT, check=True, timeout=90)

    flags = ["--target=x86_64-pc-win32-coff", "-ffreestanding", "-fno-builtin",
             "-fno-stack-protector", "-fno-unwind-tables", "-fno-asynchronous-unwind-tables",
             "-mno-red-zone", "-mgeneral-regs-only", "-Wall", "-Wextra", "-Werror"]
    objects = []
    for source in ("boot/uefi/loader.c", "boot/uefi/runtime.c", "lib/boot/contract.c",
                   "lib/boot/memory.c", "lib/boot/loader.c"):
        target = output / (Path(source).stem + ("-efi" if source == "boot/uefi/loader.c" else "") + ".obj")
        run([clang, *flags, "-std=gnu11" if source.startswith("boot/uefi/") else "-std=c11",
             "-Iinclude", "-Iboot/uefi", "-c", source, "-o", target])
        objects.append(target)
    entry, trampoline = output / "entry.obj", output / "trampoline.obj"
    run([nasm, "--reproducible", "-f", "win64", "-o", entry, "boot/uefi/entry.asm"])
    run([nasm, "--reproducible", "-f", "win64", "-o", trampoline, "boot/uefi/trampoline.asm"])
    image = output / "BOOTX64.EFI"
    run([linker, "/subsystem:efi_application", "/entry:efi_entry", "/nodefaultlib",
         "/machine:x64", "/dynamicbase", "/nxcompat", "/out:" + str(image),
         entry, trampoline, *objects])
    report = {"status": "PASS", "mode": "build-only", "tool_versions": {
        "clang": subprocess.check_output([clang, "--version"], text=True).splitlines()[0],
        "lld-link": subprocess.check_output([linker, "--version"], text=True).strip(),
        "nasm": subprocess.check_output([nasm, "-v"], text=True).strip()},
        "pe": inspect(image), "commands": commands,
        "source_hashes": {name: hashlib.sha256((ROOT / name).read_bytes()).hexdigest()
                          for name in SOURCES}}
    (output / "build.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"PASS: PE/COFF EFI build-only, {report['pe']['bytes']} bytes; chưa chứng minh firmware boot")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=Path, required=True)
    args = parser.parse_args()
    build(args.build)
