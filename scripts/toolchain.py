# SPDX-License-Identifier: GPL-3.0-only
"""Tiện ích host dùng chung; không cấu hình hoặc cài đặt môi trường."""

import os
from pathlib import Path
import shutil
import subprocess


def run(command, timeout=15):
    result = subprocess.run(command, capture_output=True, text=True, timeout=timeout)
    if result.returncode:
        raise ValueError(f"{command[0]} exit {result.returncode}: {result.stderr.strip()[:300]}")
    return result.stdout.strip()


def cross_tools():
    prefix = os.environ.get("LAMIX_CROSS_ROOT")
    gcc = Path(prefix) / "bin/x86_64-elf-gcc" if prefix else Path(shutil.which("x86_64-elf-gcc") or "/missing/x86_64-elf-gcc")
    gcc = gcc.absolute()
    tools = {name: gcc.parent / f"x86_64-elf-{name}" for name in (
        "gcc", "g++", "ld", "as", "ar", "ranlib", "nm", "readelf", "objdump", "objcopy",
    )}
    for name, path in tools.items():
        if not path.is_file() or not os.access(path, os.X_OK):
            raise ValueError(f"Thiếu cross tool {name}: {path}")
    for compiler in ("gcc", "g++"):
        if run([str(tools[compiler]), "-dumpmachine"]) != "x86_64-elf":
            raise ValueError(f"{compiler}: target phải là x86_64-elf, không dùng compiler host")
    linker = Path(run([str(gcc), "-print-prog-name=ld"])).resolve()
    if not linker.is_relative_to(gcc.parent.parent.resolve()):
        raise ValueError("GCC đang dùng linker ngoài prefix cross")
    return tools
