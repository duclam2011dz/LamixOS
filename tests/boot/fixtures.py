# SPDX-License-Identifier: GPL-3.0-only
"""Golden bytes viết bằng struct.pack theo contract; không dùng C structs."""
import struct as S

def record(typ, payload, required=True):
    payload += bytes((-len(payload)) % 8)
    return S.pack('<IIQ', typ, int(required), 16+len(payload)) + payload

def table(typ, entries, extra=b'', required=True):
    return record(typ, extra+S.pack('<IIQ', 1, 32, len(entries))+b''.join(entries), required)

def segment(phys, virt, size, perms):
    return S.pack('<QQQII', phys, virt, size, perms, 0)

def reservation(base, size, purpose, release):
    return S.pack('<QQIIQ', base, size, purpose, release, 0)

def bootinfo(source=1, extras=(), features=0, memory=None, reservations=None):
    # Allocations tách biệt theo page; không hề tạo entry code hoặc image.
    memory = memory if memory is not None else [(0x100000, 0x40000, 2, 0, 0),
                                               (0x200000, 0x10000, 1, 0, 0),
                                               (0x100000000, 0x1000, 1, 0, 0)]
    reservations = reservations if reservations is not None else [
        reservation(0x100000, 4096, 1, 0), reservation(0x110000, 4096, 3, 1),
        reservation(0x120000, 65536, 4, 2), reservation(0x130000, 4096, 5, 2)]
    records = [table(1, [S.pack('<QQIIQ', *m) for m in memory]),
               table(2, [segment(0x100000, 0xffffffff80000000, 4096, 5)], S.pack('<Q', 0xffffffff80000000)),
               table(3, reservations),
               table(4, [segment(0x100000, 0xffffffff80000000, 4096, 5),
                         segment(0x110000, 0x110000, 4096, 1),
                         segment(0x120000, 0x120000, 65536, 3)],
                     S.pack('<QQQQQ', 0x130000, 0x120000, 65536, 0x12fff8, 0x11f000)),
               record(5, S.pack('<II', source, 512)+bytes([1])*16+bytes([2])*16+bytes([3])*16+S.pack('<II', 1, 0))]
    records += list(extras)
    total = 64+sum(map(len, records))
    return bytearray(S.pack('<8sHHIQQQQIIQ', b'LAMIXLBP', 1, 0, 64, total, 0, features, 64, len(records), 0, 0x110000)+b''.join(records))

def offsets(blob):
    result = {}; at = S.unpack_from('<Q', blob, 40)[0]
    for _ in range(S.unpack_from('<I', blob, 48)[0]):
        typ, _, size = S.unpack_from('<IIQ', blob, at); result[typ] = at; at += size
    return result
