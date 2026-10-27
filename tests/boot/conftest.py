# SPDX-License-Identifier: GPL-3.0-only
import ctypes as C
import os
from pathlib import Path
import subprocess
import pytest

ROOT = Path(__file__).resolve().parents[2]

class Memory(C.Structure):
    _fields_ = [('base', C.c_uint64), ('length', C.c_uint64), ('type', C.c_uint32),
                ('flags', C.c_uint32), ('attrs', C.c_uint64)]

class Reservation(C.Structure):
    _fields_ = [('base', C.c_uint64), ('length', C.c_uint64), ('purpose', C.c_uint32),
                ('release', C.c_uint32), ('reserved', C.c_uint64)]

class View(C.Structure):
    _fields_ = [('bytes', C.c_void_p), ('size', C.c_uint64), ('count', C.c_uint32)]

class Span(C.Structure):
    _fields_ = [('physical', C.c_uint64), ('bytes', C.c_void_p), ('size', C.c_uint64)]

@pytest.fixture(scope='session')
def lib(tmp_path_factory):
    path = os.environ.get('LAMIX_CONTRACT_LIBRARY')
    if not path:
        path = str(tmp_path_factory.mktemp('m03') / 'contract.so')
        subprocess.run(['gcc', '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror', '-shared', '-fPIC',
                        '-I', str(ROOT / 'include'), str(ROOT / 'lib/boot/contract.c'),
                        str(ROOT / 'lib/boot/memory.c'), '-o', path], check=True)
    dll = C.CDLL(path)
    u64, u32, ptr = C.c_uint64, C.c_uint32, C.c_void_p
    dll.lbp_validate.argtypes = [ptr, u64, C.POINTER(Span), u64, C.POINTER(View)]
    dll.lbp_validate_note.argtypes = [ptr, u64, u64]
    dll.lbp_record_at.argtypes = [C.POINTER(View), u32, C.POINTER(u64)]
    dll.lbp_decode_e820.argtypes = [ptr, u64, u32, C.POINTER(Memory), u64, C.POINTER(u64)]
    dll.lbp_decode_uefi.argtypes = [ptr, u64, u32, u32, C.POINTER(Memory), u64, C.POINTER(u64)]
    dll.lbp_normalize.argtypes = [C.POINTER(Memory), u64, C.POINTER(Reservation), u64,
                                 C.POINTER(Memory), u64, C.POINTER(Memory), u64, C.POINTER(u64)]
    return dll
