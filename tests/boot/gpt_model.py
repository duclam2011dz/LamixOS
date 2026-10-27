# SPDX-License-Identifier: GPL-3.0-only
"""Mô hình metadata host M03; không file/disk I/O, không tạo bootable image."""
import struct
import uuid
import zlib

MIB = 1024 * 1024
BIOS = uuid.UUID('21686148-6449-6e6f-744e-656564454649')
ESP = uuid.UUID('c12a7328-f81f-11d2-ba4b-00a0c93ec93b')
SYSTEM = uuid.UUID('b2c2410b-b210-56ec-bdc3-589072b4fe3d')
USER = uuid.UUID('881eed6f-b4b9-5e1b-b0bf-c0a4f1eb2dd9')
ROLES = ('BIOS Boot', 'ESP', 'System A', 'System B', 'User Data')
TYPES = (BIOS, ESP, SYSTEM, SYSTEM, USER)


def layout(size=8 * 1024**3, block=512):
    if block not in (512, 4096) or size % block or size >= 2**64 or size <= 0:
        raise ValueError('size/block không hỗ trợ')
    blocks = size // block
    array_blocks = 16384 // block
    first, last = 2 + array_blocks, blocks - array_blocks - 2
    starts = [x * MIB // block for x in (1, 3, 259, 2307, 4355)]
    ends = [starts[i + 1] - 1 for i in range(4)] + [last]
    if starts[0] < first or (last - starts[4] + 1) * block < MIB:
        raise ValueError('disk quá nhỏ')
    return dict(block=block, blocks=blocks, first=first, last=last,
                primary_array=2, backup_array=blocks-array_blocks-1,
                partitions=list(zip(starts, ends)))


def validate_pmbr(data, blocks, block):
    if block not in (512, 4096) or len(data) != block or blocks < 2:
        raise ValueError('PMBR size')
    if data[440:446] != bytes(6) or data[510:512] != b'\x55\xaa' or any(data[512:]):
        raise ValueError('PMBR reserved/signature')
    if data[446:450] != b'\0\0\2\0' or data[450] != 0xee or data[451:454] != b'\xff'*3:
        raise ValueError('PMBR record')
    if struct.unpack_from('<II', data, 454) != (1, min(blocks-1, 0xffffffff)) or any(data[462:510]):
        raise ValueError('PMBR bounds/hybrid')


def validate(primary, backup, entries, backup_entries, pmbr, size, block):
    spec = layout(size, block)
    validate_pmbr(pmbr, spec['blocks'], block)
    if len(entries) != 16384 or entries != backup_entries:
        raise ValueError('GPT arrays khác nhau/size')
    headers = []
    for data, current, other, array in ((primary, 1, spec['blocks']-1, 2),
                                      (backup, spec['blocks']-1, 1, spec['backup_array'])):
        if len(data) != block:
            raise ValueError('GPT block size')
        fields = struct.unpack_from('<8sIIIIQQQQ16sQIII', data)
        sig, rev, hs, crc, reserved, cur, alt, first, last, guid, arr, count, stride, arrcrc = fields
        copy = bytearray(data[:92]); copy[16:20] = bytes(4)
        if (sig, rev, hs, reserved, cur, alt, first, last, arr, count, stride) != (
                b'EFI PART', 0x10000, 92, 0, current, other, spec['first'], spec['last'], array, 128, 128):
            raise ValueError('GPT header fields')
        if any(data[92:]) or not any(guid) or zlib.crc32(copy) != crc or zlib.crc32(entries) != arrcrc:
            raise ValueError('GPT CRC/reserved/GUID')
        headers.append(guid)
    if headers[0] != headers[1]:
        raise ValueError('GPT disk GUID mismatch')
    seen = {headers[0]}
    for i in range(128):
        entry = entries[i*128:(i+1)*128]
        if i >= 5:
            if any(entry):
                raise ValueError('GPT unused entry')
            continue
        typ, ident, start, end, attrs, name = struct.unpack('<16s16sQQQ72s', entry)
        if typ != TYPES[i].bytes_le or not any(ident) or ident in seen or attrs:
            raise ValueError('GPT type/unique GUID/attributes')
        seen.add(ident)
        if (start, end) != spec['partitions'][i] or start > end or not spec['first'] <= start <= end <= spec['last']:
            raise ValueError('GPT partition bounds/overlap/alignment')
        if name != ROLES[i].encode('utf-16le').ljust(72, b'\0'):
            raise ValueError('GPT name')
    return spec
