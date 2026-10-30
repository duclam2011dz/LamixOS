#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Chạy benchmark native M12 và lưu JSON kết quả host."""
import argparse
import json
from pathlib import Path
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--benchmark", type=Path, required=True)
parser.add_argument("--fixture", type=Path, required=True)
parser.add_argument("--output", type=Path, required=True)
args = parser.parse_args()
result = subprocess.run([str(args.benchmark), str(args.fixture)], check=True,
                        capture_output=True, text=True, timeout=120)
document = json.loads(result.stdout)
args.output.parent.mkdir(parents=True, exist_ok=True)
args.output.write_text(json.dumps(document, indent=2) + "\n", encoding="utf-8")
print(json.dumps(document, ensure_ascii=False))
