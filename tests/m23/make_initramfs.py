#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Tạo archive initramfs newc xác định cho profile M23."""
import argparse
from pathlib import Path


def record(name: str, data: bytes, ino: int) -> bytes:
    encoded = name.encode("ascii") + b"\0"
    fields = (ino, 0o100644, 0, 0, 1, 0, len(data), 0, 0, 0, 0,
              len(encoded), 0)
    header = b"070701" + b"".join(f"{field:08x}".encode("ascii")
                                  for field in fields)
    result = bytearray(header)
    result.extend(encoded)
    result.extend(b"\0" * (-len(result) % 4))
    result.extend(data)
    result.extend(b"\0" * (-len(result) % 4))
    return bytes(result)


def create(output: Path, entries: list[tuple[str, Path]]) -> None:
    archive = bytearray()
    seen = set()
    for ino, (name, path) in enumerate(entries, 1):
        if not name or "/" in name or name in seen or name == "TRAILER!!!":
            raise ValueError(f"tên initramfs không hợp lệ: {name!r}")
        seen.add(name)
        archive.extend(record(name, path.read_bytes(), ino))
    archive.extend(record("TRAILER!!!", b"", len(entries) + 1))
    archive.extend(b"\0" * (-len(archive) % 512))
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(archive)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--entry", nargs=2, action="append", metavar=("NAME", "FILE"),
                        required=True)
    args = parser.parse_args()
    create(args.output, [(name, Path(path)) for name, path in args.entry])
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
