#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Tạo BootInfo fixtures cho M12 host tests và benchmark."""
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tests/boot"))
from fixtures import bootinfo

out = Path(sys.argv[1])
out.mkdir(parents=True, exist_ok=True)
(out / "small.bin").write_bytes(bootinfo())
(out / "large.bin").write_bytes(bootinfo(memory=[
    (0x100000, 0x40000, 2, 0, 0),
    (0x200000, 0x10000, 1, 0, 0),
    (0x40000000, 0x40000000, 1, 0, 0),
    (0x100000000, 0x1000, 1, 0, 0),
]))
(out / "adjacent.bin").write_bytes(bootinfo(memory=[
    (0x100000, 0x40000, 2, 0, 0),
    (0x200000, 0x2000, 1, 0, 0),
    (0x202000, 0x2000, 1, 0, 0),
    (0x100000000, 0x1000, 1, 0, 0),
]))
(out / "holes.bin").write_bytes(bootinfo(memory=[
    (0x100000, 0x40000, 2, 0, 0),
    (0x200000, 0x2000, 1, 0, 0),
    (0x300000, 0x2000, 1, 0, 0),
    (0x100000000, 0x1000, 1, 0, 0),
]))
(out / "bench.bin").write_bytes(bootinfo(memory=[
    (0x100000, 0x40000, 2, 0, 0),
    (0x200000, 0x1000000, 1, 0, 0),
    (0x100000000, 0x1000, 1, 0, 0),
]))
