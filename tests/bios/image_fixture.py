# SPDX-License-Identifier: GPL-3.0-only
"""Builder sparse image chỉ dùng tests M04; không formatter/generator sản phẩm."""
import hashlib
from pathlib import Path
import struct
import subprocess
import sys
import zlib

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tests/boot'))
from gpt_fixture import fixtures
from gpt_model import validate, MIB


def metadata(variant='valid'):
    data = fixtures()
    validate(*data)
    primary, backup, entries, _, pmbr, size, block = data
    start, end = 2048, 6143
    if variant == 'relocated':
        # Synthetic GPT ngoài canonical profile; chứng minh tìm GUID/LBA thật.
        start = 4355 * MIB // 512
        end = start + 4095
        entry = bytearray(entries[:128])
        struct.pack_into('<QQ', entry, 32, start, end)
        entries[:128] = bytes(128)
        entries[127*128:] = entry
        struct.pack_into('<Q', entries, 4*128+32, end+1)
    elif variant == 'exact-window':
        end = start + 63
        struct.pack_into('<Q', entries, 40, end)
    elif variant.startswith('header-byte-'):
        index = int(variant.removeprefix('header-byte-'))
        if index not in list(range(16))+list(range(72,88)):
            raise ValueError('header index')
        primary[index] ^= 1
    elif variant.startswith('guid-'):
        index = int(variant.split('-')[1])
        if not 0 <= index < 16:
            raise ValueError('GUID index')
        entries[index] ^= 1
    elif variant == 'missing':
        entries[:128] = bytes(128)
    elif variant in HEADER_MUTATIONS:
        off, fmt, value = HEADER_MUTATIONS[variant]
        struct.pack_into('<'+fmt, primary, off, value)
    elif variant in PARTITION_MUTATIONS:
        off, fmt, value = PARTITION_MUTATIONS[variant]
        struct.pack_into('<'+fmt, entries, off, value)
    elif variant not in ('valid', 'payload', 'trailer'):
        raise ValueError('variant không hợp lệ')
    data[3] = bytearray(entries)
    for header in (primary, backup):
        struct.pack_into('<I', header, 88, zlib.crc32(entries))
        header[16:20] = bytes(4)
        struct.pack_into('<I', header, 16, zlib.crc32(header[:92]))
    if variant == 'relocated':
        # Kiểm tra độc lập CRC và bounds/overlap dù test đổi thứ tự partitions.
        assert primary[88:92] == backup[88:92]
        ranges = []
        for i in range(128):
            e = entries[i*128:(i+1)*128]
            if not any(e):
                continue
            a, b = struct.unpack_from('<QQ', e, 32)
            assert 34 <= a <= b <= size//512-34
            assert all(b < x or a > y for x, y in ranges)
            ranges.append((a, b))
        assert len(ranges) == 5
    return data, start, end


HEADER_MUTATIONS = {
    'signature': (0, 'B', 0), 'revision': (8, 'I', 0x20000),
    'header-size': (12, 'I', 96), 'table-lba': (72, 'Q', 3),
    'table-high': (76, 'I', 1), 'entry-count': (80, 'I', 127),
    'entry-size': (84, 'I', 256),
}
PARTITION_MUTATIONS = {
    'start-low': (32, 'Q', 33), 'end-before': (40, 'Q', 2047),
    'start-high': (36, 'I', 1), 'end-high': (44, 'I', 1),
    'too-small': (40, 'Q', 2048+62), 'backup-overlap': (40, 'Q', 8*1024**3//512-33),
    'wrap': (32, 'Q', 0xfffffff0),
}


def assemble_probe(destination, start, end):
    subprocess.run(['nasm', '--reproducible', '-f', 'bin',
                    f'-DEXPECTED_START={start}', f'-DEXPECTED_END={end}',
                    '-o', str(destination), str(ROOT/'tests/bios/stage_probe.asm')],
                   check=True, capture_output=True, text=True)
    probe = Path(destination).read_bytes()
    if len(probe) != 32768:
        raise ValueError('Probe size')
    return probe


def create_image(destination, stage1, probe, variant='valid'):
    target = Path(destination).absolute()
    resolved = target.resolve()
    if not any(resolved.is_relative_to(base) for base in
               (ROOT/'out', ROOT/'build', Path('/tmp'))):
        raise ValueError('image chỉ trong build/out/tmp test')
    if len(stage1) != 440 or len(probe) != 32768:
        raise ValueError('Stage1/probe size')
    data, start, end = metadata(variant)
    primary, backup, entries, back_entries, pmbr, size, block = data
    payload = bytearray(probe)
    if variant == 'payload':
        payload[5*512+8] ^= 1
    if variant == 'trailer':
        payload[-1] ^= 1
    pmbr[:440] = stage1
    extents = [
        ('pmbr', 0, bytes(pmbr)), ('primary', 512, bytes(primary)),
        ('entries', 1024, bytes(entries)),
        ('backup-entries', size-33*512, bytes(back_entries)),
        ('backup', size-512, bytes(backup)),
        ('probe', start*512, bytes(payload)),
    ]
    for index, name in [(1, 'ESP'), (2, 'SystemA'), (3, 'SystemB'), (4, 'User')]:
        off = struct.unpack_from('<Q', entries, index*128+32)[0]*512
        extents.append((name, off, ('M04 protected '+name).encode().ljust(64,b'\0')))
    # xb không ghi đè file/symlink/device đã có; chỉ truncate file vừa tạo.
    with target.open('xb') as image:
        image.truncate(size)
        for name, offset, content in extents:
            image.seek(offset)
            image.write(content)
    if target.stat().st_blocks*512 >= MIB:
        raise ValueError('fixture không sparse như yêu cầu')
    return {'size': size, 'start': start, 'end': end,
            'extents': [(name, offset, len(content)) for name,offset,content in extents]}


def extent_hashes(image, manifest):
    result = {}
    with Path(image).open('rb') as source:
        for name, offset, length in manifest['extents']:
            source.seek(offset)
            result[name] = hashlib.sha256(source.read(length)).hexdigest()
    return result
