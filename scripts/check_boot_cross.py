#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Compile-only M03, kiểm tra compiler provenance và runtime symbols; không boot."""
import json
from pathlib import Path
import shlex
import subprocess
import sys

try:
    from .toolchain import cross_tools
except ImportError:
    from toolchain import cross_tools

ROOT = Path(__file__).resolve().parents[1]


def main():
    output = ROOT / 'out/m03'
    output.mkdir(parents=True, exist_ok=True)
    build = ROOT / 'build/m03-cross'
    report = {'mode': 'cross-compile-only', 'status': 'FAIL'}
    try:
        tools = cross_tools()
        with (output / 'cross.log').open('w') as log:
            commands = [
                ['cmake', '-S', str(ROOT / 'tests/boot'), '-B', str(build), '-G', 'Ninja',
                 '-DCMAKE_BUILD_TYPE=Release', '-DCMAKE_EXPORT_COMPILE_COMMANDS=ON',
                 f'-DCMAKE_TOOLCHAIN_FILE={ROOT}/cmake/x86_64-elf.cmake'],
                ['cmake', '--build', str(build), '--verbose'],
            ]
            for command in commands:
                log.write('$ ' + ' '.join(command) + '\n')
                log.flush()
                subprocess.run(command, stdout=log, stderr=subprocess.STDOUT,
                               check=True, timeout=90)
        commands = json.loads((build / 'compile_commands.json').read_text())
        if len(commands) != 4:
            raise ValueError('cần đúng 4 compile commands thư viện/assertions C/C++')
        for command in commands:
            args = shlex.split(command['command'])
            expected = tools['g++'] if command['file'].endswith('.cpp') else tools['gcc']
            if Path(args[0]).resolve() != expected.resolve():
                raise ValueError('CMake cache/compiler không khớp cross-toolchain đã chọn')
            for flag in ('-ffreestanding', '-fno-builtin', '-mgeneral-regs-only', '-mno-red-zone'):
                if flag not in args:
                    raise ValueError(f'thiếu target flag {flag}')
            if any('/usr/include' in arg or '/usr/lib' in arg for arg in args):
                raise ValueError('compile command có đường dẫn runtime host')
        result = subprocess.run([str(tools['nm']), '-u', str(build / 'libboot-contract.a')],
                                capture_output=True, text=True, check=True)
        undefined = [line for line in result.stdout.splitlines()
                     if line.strip() and not line.endswith(':')]
        if undefined:
            raise ValueError(f'undefined symbols: {undefined}')
        report.update(status='PASS', undefined_symbols=[], compiler=str(tools['gcc']),
                      target='x86_64-elf', archive=str(build / 'libboot-contract.a'),
                      verified_compile_commands=len(commands))
    except (OSError, ValueError, KeyError, subprocess.SubprocessError) as exc:
        report['error'] = str(exc)
    (output / 'cross.json').write_text(json.dumps(report, indent=2) + '\n')
    print(report['status'] + ': M03 cross compile-only; xem out/m03/cross.log và cross.json')
    return int(report['status'] != 'PASS')


if __name__ == '__main__':
    sys.exit(main())
