#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Build profile M19 BSP preemptive scheduler trên baseline M17."""
import argparse
from pathlib import Path
import sys
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tests/m08"))
from build_kernel import build

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=Path, required=True)
    args = parser.parse_args()
    build(args.build, m19_scheduler=True)

if __name__ == "__main__":
    main()
