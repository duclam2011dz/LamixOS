#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Build kernel M10 và provenance; artifacts chỉ ghi trong build/."""
import argparse
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tests/m08"))
from build_kernel import build


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--fault", choices=("none", "ro", "nx", "guard"),
                        default="none")
    args = parser.parse_args()
    build(args.build, fault=args.fault, m10_vmm=True)


if __name__ == "__main__":
    main()
