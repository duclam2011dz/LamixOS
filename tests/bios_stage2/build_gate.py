#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Gate C32/C64 compile/link, không tạo image hoặc chạy target ELF trên host."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from scripts.toolchain import cross_tools

STAGE1_HASH = 'a63736f43692aa428de888e6fd10979f098be29084fa79a8e4ef7ca1449296c4'


def inspect_elf(path, bits):
    data = path.read_bytes()
    if data[:7] != b'\x7fELF' + bytes([1 if bits == 32 else 2, 1, 1]):
        raise ValueError('ELF class/encoding/version')
    fmt = '<HHIIIIIHHHHHH' if bits == 32 else '<HHIQQQIHHHHHH'
    fields = struct.unpack_from(fmt, data, 16)
    typ, machine, _, entry, phoff = fields[:5]
    phsize, count = fields[8:10]
    if typ != 2 or machine != (3 if bits == 32 else 62):
        raise ValueError('ELF type/machine')
    loads = []
    notes = []
    for i in range(count):
        at = phoff + i * phsize
        if bits == 32:
            ptype, off, virt, phys, size, memory, flags, align = struct.unpack_from('<IIIIIIII', data, at)
        else:
            ptype, flags, off, virt, phys, size, memory, align = struct.unpack_from('<IIQQQQQQ', data, at)
        if ptype in (2, 3, 7):
            raise ValueError('dynamic/interpreter/TLS không được phép')
        if ptype == 1:
            if size > memory or off + size > len(data) or flags & 3 == 3:
                raise ValueError('PT_LOAD bounds/WX')
            if bits == 64 and (phys or virt < 0xffffffff80000000):
                raise ValueError('probe phải higher-half và p_paddr=0')
            loads.append(dict(virtual=virt, physical=phys, filesz=size, memsz=memory, flags=flags))
        if ptype == 4:
            notes.append(data[off:off+size])
    if not loads or not any(x['virtual'] <= entry < x['virtual']+x['memsz'] and x['flags'] & 1 for x in loads):
        raise ValueError('ELF entry không executable')
    if bits == 64 and (len(notes) != 1 or b'LAMIXREQ' not in notes[0]):
        raise ValueError('probe request note')
    return dict(bits=bits, entry=entry, loads=loads, bytes=len(data))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--profile', choices=('Debug', 'Release'), default='Debug')
    parser.add_argument('--verify', action='store_true')
    args = parser.parse_args()
    build = args.build.resolve()
    if not build.is_relative_to(ROOT / 'build'):
        raise ValueError('build gate chỉ ghi dưới build/')
    tools = cross_tools()
    build.mkdir(parents=True, exist_ok=True)
    report_path = build / 'gate.json'
    log_path = build / 'gate.log'
    if args.verify:
        report = json.loads(report_path.read_text())
        if report['status'] != 'PASS':
            raise ValueError('gate chưa PASS: ' + report.get('error', 'thiếu kết quả'))
        for name, expected in report['hashes'].items():
            if hashlib.sha256((build / name).read_bytes()).hexdigest() != expected:
                raise ValueError('gate artifact đã đổi: ' + name)
        if report['status'] != 'PASS':
            raise ValueError('gate chưa PASS')
        print('PASS: compile/link-only C32/C64; chưa có boot proof')
        return
    report = dict(status='FAIL', mode='compile-link-only', profile=args.profile)
    with log_path.open('w') as log:
        def run(command):
            command = [str(x) for x in command]
            log.write('$ ' + ' '.join(command) + '\n'); log.flush()
            return subprocess.run(command, stdout=log, stderr=subprocess.STDOUT,
                                  check=True, timeout=60, cwd=ROOT)
        try:
            flags = ['-std=c11', '-ffreestanding', '-fno-builtin', '-fno-stack-protector',
                     '-fno-pie', '-fno-unwind-tables', '-fno-asynchronous-unwind-tables',
                     '-mgeneral-regs-only', '-Wall', '-Wextra', '-Werror', '-I'+str(ROOT/'include'),
                     '-O0' if args.profile == 'Debug' else '-O2']
            objects = []
            for src in ['tests/bios_stage2/gate.c', 'lib/boot/contract.c', 'lib/boot/memory.c',
                        'boot/bios/stage2/div64.c', 'tests/bios_stage2/layout.c', 'tests/bios_stage2/layout.cpp']:
                dest = build / (Path(src).name + '.o')
                compiler = tools['g++'] if src.endswith('.cpp') else tools['gcc']
                local_flags = [x for x in flags if not x.startswith('-std=')]
                local_flags += ['-std=c++17', '-fno-exceptions', '-fno-rtti'] if src.endswith('.cpp') else ['-std=c11']
                run([compiler, *local_flags, '-m32', '-malign-double', '-DLAMIX_I386_RUNTIME=1', '-c', src, '-o', dest])
                objects.append(dest)
            run([tools['ld'], '-m', 'elf_i386', '--fatal-warnings', '--build-id=none', '-z', 'noexecstack',
                 '-T', ROOT/'boot/bios/stage2/core.ld', '-Map', build/'gate-core.map', '-o', build/'gate-core.elf', *objects])
            run(['nasm', '--reproducible', '-f', 'elf64', '-o', build/'gate-probe.o', 'tests/bios_stage2/gate_probe.asm'])
            run([tools['gcc'], *flags, '-m64', '-mcmodel=kernel', '-mno-red-zone', '-c', 'lib/boot/contract.c', '-o', build/'contract64.o'])
            run([tools['g++'], *[x for x in flags if not x.startswith('-std=')], '-std=c++17',
                 '-fno-exceptions', '-fno-rtti', '-m64', '-mno-red-zone', '-c', 'tests/bios_stage2/layout.cpp', '-o', build/'layout64.o'])
            run([tools['ld'], '--fatal-warnings', '--build-id=none', '-z', 'noexecstack',
                 '-T', ROOT/'tests/bios_stage2/probe.ld', '-o', build/'gate-probe.elf',
                 build/'gate-probe.o', build/'contract64.o', build/'layout64.o'])
            # LD gán p_paddr theo LMA. Profile Lamix không dùng LMA trong ELF;
            # producer test chuyển đúng field đó về zero, không đổi section,
            # virtual addresses hoặc bytes của segment. Không tắt link checks.
            probe = bytearray((build/'gate-probe.elf').read_bytes())
            phoff = struct.unpack_from('<Q', probe, 32)[0]
            phsize, phcount = struct.unpack_from('<HH', probe, 54)
            for i in range(phcount):
                at = phoff+i*phsize
                if struct.unpack_from('<I', probe, at)[0] == 1:
                    struct.pack_into('<Q', probe, at+24, 0)
            (build/'gate-probe.elf').write_bytes(probe)
            for name in ('gate-core.elf', 'gate-probe.elf'):
                undefined = subprocess.check_output([str(tools['nm']), '-u', str(build/name)], text=True)
                if undefined.strip():
                    raise ValueError('undefined symbols: '+undefined)
            run(['nasm', '--reproducible', '-f', 'bin', '-o', build/'stage1.bin', 'boot/bios/stage1.asm'])
            if len((build/'stage1.bin').read_bytes()) != 440 or hashlib.sha256((build/'stage1.bin').read_bytes()).hexdigest() != STAGE1_HASH:
                raise ValueError('Stage1 M04 thay đổi')
            report.update(status='PASS', compiler=str(tools['gcc']),
                          core=inspect_elf(build/'gate-core.elf', 32), probe=inspect_elf(build/'gate-probe.elf', 64),
                          hashes={n:hashlib.sha256((build/n).read_bytes()).hexdigest()
                                  for n in ('gate-core.elf','gate-probe.elf','stage1.bin')})
        except (OSError, ValueError, subprocess.SubprocessError) as error:
            report['error'] = str(error)
        report_path.write_text(json.dumps(report, indent=2)+'\n')
    print(report['status'] + ': gate compile/link; ' + str(log_path))
    if report['status'] != 'PASS':
        raise SystemExit(1)


if __name__ == '__main__':
    main()
