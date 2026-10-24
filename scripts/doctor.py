#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Chẩn đoán dependencies M01; không tự cài hoặc thay đổi host."""

import argparse
import json
import os
from pathlib import Path
import platform
import re
import shutil
import subprocess
import sys

try:
    from .toolchain import cross_tools, run
except ImportError:
    from toolchain import cross_tools, run


def check_command(name, command, minimum=None, required=True):
    try:
        output = run(command)
        if minimum:
            match = re.search(r"\d+\.\d+(?:\.\d+)?", output)
            version = tuple(map(int, match[0].split("."))) if match else ()
            if version < minimum:
                raise ValueError(f"cần phiên bản >= {'.'.join(map(str, minimum))}")
        return {"name": name, "status": "PASS", "detail": output.splitlines()[0] if output else "exit 0", "path": str(command[0])}
    except (OSError, ValueError, subprocess.TimeoutExpired) as exc:
        return {"name": name, "status": "FAIL" if required else "WARN", "detail": str(exc)}


def diagnose(profile):
    results = []
    valid_host = platform.system() == "Linux" and platform.machine() == "x86_64"
    if profile == "wsl2":
        valid_host = valid_host and "microsoft-standard-wsl2" in platform.release().lower()
    results.append({"name": "host", "status": "PASS" if valid_host else "FAIL", "detail": f"{platform.system()} {platform.machine()} {platform.release()} ({profile})"})
    specs = {
        "cmake": ("--version", (3, 22)), "ninja": ("--version", (1, 10)),
        "ctest": ("--version", (3, 22)), "nasm": ("-v", (2, 15)),
        "qemu-system-x86_64": ("--version", (6, 2)), "gdb": ("--version", (12, 1)),
        "clang-format": ("--version", (14, 0)), "clang-tidy": ("--version", (14, 0)),
        "cppcheck": ("--version", (2, 7)),
    }
    for name, (flag, version) in specs.items():
        results.append(check_command(name, [shutil.which(name) or name, flag], version))
    results.append(check_command("python", [sys.executable, "--version"], (3, 10)))
    results.append(check_command("pytest", [sys.executable, "-m", "pytest", "--version"], (8, 3, 5)))
    try:
        for name, path in cross_tools().items():
            item = check_command(f"cross-{name}", [str(path), "--version"])
            if name in ("gcc", "g++") and run([str(path), "-dumpfullversion"]) != "12.2.0":
                item.update(status="FAIL", detail="M01 yêu cầu GCC 12.2.0 theo manifest")
            if name not in ("gcc", "g++") and "2.39" not in item["detail"]:
                item.update(status="FAIL", detail="M01 yêu cầu Binutils 2.39 theo manifest")
            results.append(item)
    except (OSError, ValueError, subprocess.TimeoutExpired) as exc:
        results.append({"name": "cross-toolchain", "status": "FAIL", "detail": str(exc)})
    for name, candidates in {
        "OVMF_CODE": ["/usr/share/OVMF/OVMF_CODE.fd", "/usr/share/OVMF/OVMF_CODE_4M.fd"],
        "OVMF_VARS": ["/usr/share/OVMF/OVMF_VARS.fd", "/usr/share/OVMF/OVMF_VARS_4M.fd"],
        "SeaBIOS": ["/usr/share/seabios/bios.bin"],
    }.items():
        found = next((p for p in candidates if Path(p).is_file() and os.access(p, os.R_OK)), None)
        results.append({"name": name, "status": "PASS" if found else "FAIL", "detail": found or "không có firmware đọc được; chưa chạy boot"})
    results.append(check_command("i686-elf (tùy chọn)", [shutil.which("i686-elf-gcc") or "i686-elf-gcc", "-dumpmachine"], required=False))
    kvm = os.access("/dev/kvm", os.R_OK | os.W_OK)
    results.append({"name": "KVM (tùy chọn)", "status": "PASS" if kvm else "WARN", "detail": "có quyền thiết bị" if kvm else "không bắt buộc; chưa chạy QEMU"})
    return results


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--profile", choices=("wsl2", "ci"), default="wsl2")
    parser.add_argument("--json", type=Path)
    args = parser.parse_args()
    results = diagnose(args.profile)
    for item in results:
        print(f"{item['status']}: {item['name']}: {item['detail']}")
    if args.json:
        args.json.parent.mkdir(parents=True, exist_ok=True)
        args.json.write_text(json.dumps({"profile": args.profile, "checks": results}, ensure_ascii=False, indent=2) + "\n")
    return int(any(item["status"] == "FAIL" for item in results))


if __name__ == "__main__":
    raise SystemExit(main())
