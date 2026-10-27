# SPDX-License-Identifier: GPL-3.0-only
"""Chỉ xuất BootInfo fixture host vào build directory, không phải disk image."""
from pathlib import Path
import sys
from fixtures import bootinfo
if __name__ == '__main__':
    Path(sys.argv[1]).write_bytes(bootinfo())
