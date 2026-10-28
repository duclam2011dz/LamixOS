#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Host native Debug/Release/sanitizer độc lập cross compiler và NASM."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile
from stage2_fixture import ROOT,create_stage2_image
from test_stage2_host import golden_elf

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--build',type=Path,required=True);p.add_argument('--profile',choices=['Debug','Release'],default='Debug');p.add_argument('--sanitize',action='store_true');a=p.parse_args()
    build=a.build.resolve()
    if not build.is_relative_to(ROOT/'build'):raise ValueError('host build ngoài build/')
    build.mkdir(parents=True,exist_ok=True)
    flags=['-O0','-g'] if a.profile=='Debug' else ['-O2']
    if a.sanitize:flags+=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie']
    command=['gcc','-std=c11','-D_POSIX_C_SOURCE=200809L','-Wall','-Wextra','-Werror','-I'+str(ROOT/'include'),*flags,
             str(ROOT/'tests/bios_stage2/native.c'),str(ROOT/'lib/boot/loader.c'),str(ROOT/'lib/boot/contract.c'),str(ROOT/'boot/bios/stage2/div64.c'),'-o',str(build/'native')]
    with (build/'host.log').open('w') as log:
        log.write('$ '+' '.join(command)+'\n');log.flush();subprocess.run(command,stdout=log,stderr=subprocess.STDOUT,check=True)
        with tempfile.TemporaryDirectory(prefix='m05-host-') as temp:
            tmp=Path(temp);art=tmp/'art';art.mkdir();(art/'stage1.bin').write_bytes(bytes(440));(art/'stage2.bin').write_bytes(bytes(33792));(art/'kernel.elf').write_bytes(golden_elf())
            run=[str(build/'native')]
            for variant,expected in [('valid','OK'),('fragmented','OK'),('gpt-backup-crc','GPT'),('gpt-array-crc','GPT'),('fat-bpb','FAT'),('fat-loop','FAT'),('fat-mirror','FAT'),('missing-a','FAT'),('elf-magic','ELF'),('note-required-acpi','ELF')]:
                path=tmp/(variant+'.img');create_stage2_image(path,art,variant);run += [str(path),expected]
            env=os.environ.copy()
            if a.sanitize:env.update(ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
            subprocess.run(run,stdout=log,stderr=subprocess.STDOUT,env=env,check=True,timeout=60)
    print('PASS: host native '+a.profile+(' ASan/UBSan' if a.sanitize else ''))
