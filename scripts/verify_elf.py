#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Kiểm tra ELF sample, link map và compiler provenance; không chạy ELF."""

import argparse
import json
from pathlib import Path
import shlex
import struct

try:
    from .toolchain import cross_tools
except ImportError:
    from toolchain import cross_tools


def verify_bytes(data):
    if len(data) < 64 or data[:7] != b"\x7fELF\x02\x01\x01":
        raise ValueError("cần ELF64 little-endian, version 1")
    header = struct.unpack_from("<HHIQQQIHHHHHH", data, 16)
    kind, machine, _, entry, phoff, shoff, _, _, phsize, phnum, shsize, shnum, _ = header
    if kind != 2 or machine != 62:
        raise ValueError("cần ET_EXEC cho x86_64")
    if phsize != 56 or shsize != 64 or not phnum or not shnum:
        raise ValueError("bảng ELF thiếu hoặc sai kích thước")
    if phoff + phnum * phsize > len(data) or shoff + shnum * shsize > len(data):
        raise ValueError("bảng ELF vượt kích thước file")
    executable_entry = False
    for i in range(phnum):
        ptype, flags, offset, address, _, size, memory_size, _ = struct.unpack_from("<IIQQQQQQ", data, phoff + i * phsize)
        if ptype in (2, 3):
            raise ValueError("không cho phép PT_DYNAMIC/PT_INTERP")
        if offset + size > len(data) or memory_size < size:
            raise ValueError("segment ELF vượt kích thước")
        if ptype == 1 and flags & 1 and address <= entry < address + memory_size:
            executable_entry = True
    if not executable_entry:
        raise ValueError("entry không nằm trong executable LOAD segment")
    sections = [struct.unpack_from("<IIQQQQIIQQ", data, shoff + i * shsize) for i in range(shnum)]
    symbols = {}
    for section in sections:
        _, stype, _, _, offset, size, link, _, _, stride = section
        if stype in (6, 11):
            raise ValueError("không cho phép dynamic section/symbol table")
        if stype != 2:
            continue
        if stride != 24 or size % 24 or offset + size > len(data) or link >= shnum:
            raise ValueError("symbol table không hợp lệ")
        strings_section = sections[link]
        strings = data[strings_section[4]:strings_section[4] + strings_section[5]]
        for pos in range(offset, offset + size, stride):
            index, _, _, section_index, value, _ = struct.unpack_from("<IBBHQQ", data, pos)
            if index >= len(strings):
                raise ValueError("symbol string index không hợp lệ")
            name = strings[index:].split(b"\0", 1)[0].decode("utf-8")
            if name and section_index == 0:
                raise ValueError(f"undefined symbol: {name}")
            if name:
                symbols[name] = value
    expected = {"m01_probe_entry", "m01_c_probe", "m01_cpp_probe"}
    if not expected.issubset(symbols) or symbols.get("m01_probe_entry") != entry:
        raise ValueError("thiếu symbol sample hoặc entry không khớp")
    return {"machine": "x86_64", "format": "ELF64", "entry": hex(entry), "symbols": sorted(expected)}


def verify_build(build):
    build = Path(build).resolve()
    report = verify_bytes((build / "m01-smoke.elf").read_bytes())
    tools = cross_tools()
    commands = json.loads((build / "compile_commands.json").read_text())
    if len(commands) != 2:
        raise ValueError("sample cần đúng hai compile commands C/C++")
    for command in commands:
        args = shlex.split(command["command"])
        expected = tools["g++"] if command["file"].endswith(".cpp") else tools["gcc"]
        if Path(args[0]).resolve() != expected.resolve():
            raise ValueError("compiler thực tế không phải cross-toolchain đã chọn")
        if any("/usr/include" in arg or "/usr/lib" in arg for arg in args):
            raise ValueError("compile command chứa include/library host")
    text = (build / "m01-smoke.map").read_text()
    loads = [line.split(maxsplit=1)[1].strip() for line in text.splitlines() if line.startswith("LOAD ")]
    if len(loads) != 3 or any(not value.endswith(("entry.o", "probe.c.obj", "probe.cpp.obj")) for value in loads):
        raise ValueError(f"link map phải chỉ chứa ba object sample: {loads}")
    report["compiler_root"] = str(tools["gcc"].parent.parent)
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("build", type=Path)
    args = parser.parse_args()
    try:
        print("PASS:", json.dumps(verify_build(args.build), ensure_ascii=False))
        return 0
    except (ValueError, OSError, struct.error) as exc:
        print(f"FAIL: {exc}")
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
