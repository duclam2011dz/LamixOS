#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""CLI M02; launch-only không chứng minh firmware hoặc OS đã boot."""

import argparse
import os
from pathlib import Path

try:
    from .qemu_harness import Config, run
except ImportError:
    from qemu_harness import Config, run


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--profile", choices=("bios", "uefi"), required=True)
    parser.add_argument("--launch-only", action="store_true")
    parser.add_argument("--purpose", choices=("os", "bios-stage1", "bios-stage2", "uefi-loader"), default="os")
    parser.add_argument("--image", type=Path)
    parser.add_argument("--success-marker")
    parser.add_argument("--panic-marker")
    parser.add_argument("--timeout", type=float)
    parser.add_argument("--output-root", type=Path, default=Config.output_root)
    parser.add_argument("--qemu", default="qemu-system-x86_64")
    parser.add_argument("--bios", type=Path, default=Config.bios)
    parser.add_argument("--ovmf-code", type=Path)
    parser.add_argument("--ovmf-vars", type=Path)
    args = parser.parse_args()
    if not args.launch_only:
        image = os.environ.get("LAMIX_TEST_IMAGE")
        args.image = args.image or (Path(image) if image else None)
        args.success_marker = args.success_marker or os.environ.get("LAMIX_TEST_SUCCESS_MARKER")
        args.panic_marker = args.panic_marker or os.environ.get("LAMIX_TEST_PANIC_MARKER")
    result = run(Config(**vars(args)))
    print(f"{result.status}: {result.profile}/{result.mode}: {result.detail}; logs={result.directory}")
    return result.exit_code


if __name__ == "__main__":
    raise SystemExit(main())
