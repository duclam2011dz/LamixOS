# SPDX-License-Identifier: GPL-3.0-only
"""Negative tests cho doctor, cross-selection, ELF và bootstrap guard."""

import io
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tarfile
import tempfile
import unittest
from unittest.mock import patch

from scripts import doctor, toolchain
from scripts.bootstrap_cross import extract
from scripts.verify_elf import verify_bytes


def elf_fixture():
    data = bytearray(1024)
    data[:16] = b"\x7fELF\x02\x01\x01" + bytes(9)
    struct.pack_into("<HHIQQQIHHHHHH", data, 16, 2, 62, 1, 0x100000, 64, 512, 0, 64, 56, 1, 64, 3, 0)
    struct.pack_into("<IIQQQQQQ", data, 64, 1, 5, 256, 0x100000, 0x100000, 1, 1, 4096)
    names = b"\0m01_probe_entry\0m01_c_probe\0m01_cpp_probe\0"
    data[300:300 + len(names)] = names
    struct.pack_into("<IIQQQQIIQQ", data, 576, 0, 3, 0, 0, 300, len(names), 0, 0, 1, 0)
    struct.pack_into("<IIQQQQIIQQ", data, 640, 0, 2, 0, 0, 384, 96, 1, 0, 8, 24)
    for i, index in enumerate((1, 17, 29), 1):
        struct.pack_into("<IBBHQQ", data, 384 + i * 24, index, 0x12, 0, 1, 0x100000, 1)
    # Dùng offset thật từ string table để fixture không phụ thuộc đếm tay.
    for i, name in enumerate((b"m01_probe_entry", b"m01_c_probe", b"m01_cpp_probe"), 1):
        struct.pack_into("<I", data, 384 + i * 24, names.index(name))
    return data


class DoctorTests(unittest.TestCase):
    def test_missing_tool_is_fail(self):
        with patch.object(doctor, "run", side_effect=FileNotFoundError("missing")):
            self.assertEqual(doctor.check_command("test", ["missing"])["status"], "FAIL")

    def test_timeout_is_fail(self):
        with patch.object(doctor, "run", side_effect=subprocess.TimeoutExpired("tool", 15)):
            self.assertEqual(doctor.check_command("test", ["tool"])["status"], "FAIL")

    def test_optional_missing_tool_is_warning(self):
        with patch.object(doctor, "run", side_effect=FileNotFoundError("missing")):
            self.assertEqual(doctor.check_command("test", ["missing"], required=False)["status"], "WARN")

    def test_old_version_is_fail(self):
        with patch.object(doctor, "run", return_value="cmake version 3.16.0"):
            self.assertEqual(doctor.check_command("cmake", ["cmake"], (3, 22))["status"], "FAIL")

    def test_host_gcc_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            for name in ("gcc", "g++", "ld", "as", "ar", "ranlib", "nm", "readelf", "objdump", "objcopy"):
                path = Path(tmp) / "bin" / f"x86_64-elf-{name}"
                path.parent.mkdir(exist_ok=True)
                path.write_text("#!/bin/sh\nexit 0\n")
                path.chmod(0o755)
            with patch.dict(os.environ, {"LAMIX_CROSS_ROOT": tmp}), patch.object(toolchain, "run", return_value="x86_64-linux-gnu"):
                with self.assertRaisesRegex(ValueError, "compiler host"):
                    toolchain.cross_tools()

    def test_explicit_missing_prefix_has_no_path_fallback(self):
        with tempfile.TemporaryDirectory() as tmp, patch.dict(os.environ, {"LAMIX_CROSS_ROOT": tmp}):
            with self.assertRaisesRegex(ValueError, "Thiếu cross"):
                toolchain.cross_tools()


class ElfTests(unittest.TestCase):
    def test_static_fixture(self):
        self.assertEqual(verify_bytes(elf_fixture())["machine"], "x86_64")

    def test_truncated_elf_rejected(self):
        with self.assertRaises(ValueError):
            verify_bytes(elf_fixture()[:50])

    def test_wrong_architecture_rejected(self):
        data = elf_fixture()
        struct.pack_into("<H", data, 18, 3)
        with self.assertRaisesRegex(ValueError, "x86_64"):
            verify_bytes(data)

    def test_dynamic_segments_rejected(self):
        for kind in (2, 3):
            data = elf_fixture()
            struct.pack_into("<I", data, 64, kind)
            with self.assertRaisesRegex(ValueError, "PT_DYNAMIC/PT_INTERP"):
                verify_bytes(data)

    def test_dynamic_sections_rejected(self):
        data = elf_fixture()
        struct.pack_into("<I", data, 580, 6)
        with self.assertRaisesRegex(ValueError, "dynamic section"):
            verify_bytes(data)

    def test_undefined_symbol_rejected(self):
        data = elf_fixture()
        struct.pack_into("<H", data, 384 + 24 + 6, 0)
        with self.assertRaisesRegex(ValueError, "undefined symbol"):
            verify_bytes(data)

    def test_table_outside_file_rejected(self):
        data = elf_fixture()
        struct.pack_into("<Q", data, 40, 100000)
        with self.assertRaisesRegex(ValueError, "vượt kích thước"):
            verify_bytes(data)


class BootstrapTests(unittest.TestCase):
    def test_archive_escape_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            archive = Path(tmp) / "bad.tar"
            with tarfile.open(archive, "w") as tar:
                member = tarfile.TarInfo("../escape")
                member.size = 1
                tar.addfile(member, io.BytesIO(b"x"))
            with self.assertRaises(ValueError):
                extract(archive, Path(tmp) / "extract")

    def test_unknown_prefix_is_not_overwritten(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "existing"
            path.write_text("keep")
            result = subprocess.run([sys.executable, "scripts/bootstrap_cross.py", "--prefix", tmp], capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertEqual(path.read_text(), "keep")


@unittest.skipUnless(shutil.which("cmake"), "cần CMake để kiểm thử configure negative")
class ConfigureTests(unittest.TestCase):
    def configure(self, prefix, build):
        root = Path(__file__).resolve().parents[1]
        env = os.environ.copy()
        env["LAMIX_CROSS_ROOT"] = str(prefix)
        return subprocess.run(["cmake", "-S", str(root), "-B", str(build), "-G", "Ninja",
                               f"-DCMAKE_TOOLCHAIN_FILE={root / 'cmake/x86_64-elf.cmake'}"],
                              env=env, capture_output=True, text=True, timeout=30)

    def test_missing_cross_compiler_stops_configure(self):
        with tempfile.TemporaryDirectory() as tmp:
            result = self.configure(Path(tmp) / "missing", Path(tmp) / "build")
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("x86_64-elf-gcc", result.stderr)

    def test_host_target_stops_configure(self):
        with tempfile.TemporaryDirectory() as tmp:
            prefix = Path(tmp) / "cross"
            compiler = prefix / "bin/x86_64-elf-gcc"
            compiler.parent.mkdir(parents=True)
            compiler.write_text("#!/bin/sh\necho x86_64-linux-gnu\n")
            compiler.chmod(0o755)
            result = self.configure(prefix, Path(tmp) / "build")
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("compiler host", result.stderr)
