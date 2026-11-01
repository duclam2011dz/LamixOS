#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Build kernel M25 VirtIO-Blk trên nền profile SMP M24."""
import argparse
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tests/m08"))
from build_kernel import build


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--max-cpus", type=int, default=16)
    args = parser.parse_args()
    if not 1 <= args.max_cpus <= 16:
        parser.error("--max-cpus phải nằm trong khoảng 1..16")
    build(args.build, m25_block=True, max_cpus=args.max_cpus)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
