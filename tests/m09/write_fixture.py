#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Tạo BootInfo M03 hợp lệ làm fixture host PMM."""
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tests/boot"))
from fixtures import bootinfo

Path(sys.argv[1]).write_bytes(bootinfo())
