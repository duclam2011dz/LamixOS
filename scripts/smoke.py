#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Build/kiểm tra M01 tại local hoặc CI; không chạy chương trình target."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

try:
    from .verify_elf import verify_build
except ImportError:
    from verify_elf import verify_build

ROOT = Path(__file__).resolve().parents[1]
INPUTS = ["CMakeLists.txt", "CMakePresets.json", "cmake/x86_64-elf.cmake",
          "tests/toolchain/entry.asm", "tests/toolchain/probe.c", "tests/toolchain/probe.cpp", "tests/toolchain/link.ld"]


def execute(command, cwd, env, log):
    log.write("$ " + " ".join(map(str, command)) + "\n")
    log.flush()
    result = subprocess.run(command, cwd=cwd, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=180)
    if result.returncode:
        raise ValueError(f"Lệnh exit {result.returncode}; xem {log.name}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--reproducible", action="store_true")
    args = parser.parse_args()
    output = ROOT / "out/m01"
    output.mkdir(parents=True, exist_ok=True)
    env = os.environ.copy()
    env["LC_ALL"] = "C"
    env["SOURCE_DATE_EPOCH"] = subprocess.check_output(["git", "show", "-s", "--format=%ct", "HEAD"], cwd=ROOT, text=True).strip()
    report = {"source_date_epoch": env["SOURCE_DATE_EPOCH"], "normal": {}, "reproducible": {}}
    try:
        for preset in ("m01-debug", "m01-release"):
            with (output / f"{preset}.log").open("w") as log:
                execute(["cmake", "--preset", preset, f"-DPython3_EXECUTABLE={sys.executable}"], ROOT, env, log)
                execute(["cmake", "--build", "--preset", preset, "--verbose"], ROOT, env, log)
                execute(["ctest", "--preset", preset, "--output-on-failure"], ROOT, env, log)
            build = ROOT / "build" / preset
            report["normal"][preset] = verify_build(build)
        if args.reproducible:
            for preset in ("m01-debug", "m01-release"):
                hashes = []
                for index in range(2):
                    # TemporaryDirectory chỉ dọn đúng thư mục do lần chạy này tạo.
                    with tempfile.TemporaryDirectory(prefix=f"repro-{index}-", dir=output) as tmp:
                        source = Path(tmp) / ("source-a" if index == 0 else "different-source-b")
                        for name in INPUTS:
                            dest = source / name
                            dest.parent.mkdir(parents=True, exist_ok=True)
                            shutil.copyfile(ROOT / name, dest)
                        with (output / f"{preset}-repro-{index}.log").open("w") as log:
                            execute(["cmake", "--preset", preset, "-DBUILD_TESTING=OFF"], source, env, log)
                            execute(["cmake", "--build", "--preset", preset], source, env, log)
                        build = source / "build" / preset
                        verify_build(build)
                        hashes.append(hashlib.sha256((build / "m01-smoke.elf").read_bytes()).hexdigest())
                report["reproducible"][preset] = hashes
                if hashes[0] != hashes[1]:
                    raise ValueError(f"SHA-256 khác nhau: {preset}: {hashes}")
        (output / "smoke.json").write_text(json.dumps(report, indent=2) + "\n")
        print("PASS: Debug/Release, CTest, ELF" + (" và SHA-256 hai source roots" if args.reproducible else ""))
        return 0
    except (OSError, ValueError, subprocess.SubprocessError) as exc:
        report["error"] = str(exc)
        (output / "smoke.json").write_text(json.dumps(report, indent=2) + "\n")
        print(f"FAIL: {exc}")
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
