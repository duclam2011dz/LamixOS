#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Build target Stage2/probe; không disk I/O ngoài artifacts build/."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys
import zlib
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT))
from scripts.toolchain import cross_tools
from build_gate import inspect_elf, STAGE1_HASH

def build(output,profile,test_fault=None,m16_video=False):
    x=cross_tools(); output=output.resolve()
    if not output.is_relative_to(ROOT/'build'):raise ValueError('output ngoài build/')
    output.mkdir(parents=True,exist_ok=True)
    (output/'build.json').write_text(json.dumps(dict(status='FAIL',mode='build-only',detail='build chưa hoàn tất'))+'\n')
    commands=[]
    with (output/'stage2-build.log').open('w') as log:
        def run(*args):
            args=list(map(str,args));commands.append(args);log.write('$ '+' '.join(args)+'\n');log.flush()
            subprocess.run(args,cwd=ROOT,stdout=log,stderr=subprocess.STDOUT,check=True,timeout=60)
        flags=['-ffreestanding','-fno-builtin','-fno-pie','-fno-stack-protector','-fno-unwind-tables','-fno-asynchronous-unwind-tables',
               '-mgeneral-regs-only','-Wall','-Wextra','-Werror','-I'+str(ROOT/'include'),'-O0' if profile=='Debug' else '-O2']
        if test_fault:flags += ['-DLAMIX_TEST_'+test_fault.upper()+'_CORRUPT=1']
        if m16_video:flags += ['-DLAMIX_M16_VIDEO=1']
        objects=[]
        for src in ['boot/bios/stage2/core.c','boot/bios/stage2/div64.c','lib/boot/contract.c','lib/boot/memory.c','lib/boot/loader.c']:
            obj=output/(Path(src).stem+'.o');objects.append(obj)
            run(x['gcc'],*flags,'-std=c11','-m32','-malign-double','-DLAMIX_I386_RUNTIME=1','-c',src,'-o',obj)
        run('nasm','--reproducible','-f','elf32','-o',output/'entry.o','boot/bios/stage2/entry.asm')
        run(x['ld'],'-m','elf_i386','--fatal-warnings','--build-id=none','-z','noexecstack','-T',ROOT/'boot/bios/stage2/core.ld',
            '-Map',output/'core.map','-o',output/'core.elf',output/'entry.o',*objects)
        run(x['objcopy'],'-O','binary',output/'core.elf',output/'core.bin')
        symbols={parts[2]:int(parts[0],16) for line in subprocess.check_output([str(x['nm']),'-n',str(output/'core.elf')],text=True).splitlines() if len(parts:=line.split())==3}
        core=(output/'core.bin').read_bytes();memory=symbols['__core_end']-0x20000
        if len(core)>memory or memory>0x40000:raise ValueError('core vượt 256KiB')
        nasm_args=['nasm','--reproducible','-f','bin','-l',output/'bootstrap.lst','-o',output/'bootstrap.bin']
        if m16_video:nasm_args += ['-D','LAMIX_M16_VIDEO=1']
        run(*nasm_args,'boot/bios/stage2/bootstrap.asm')
        bootstrap=bytearray((output/'bootstrap.bin').read_bytes())
        if len(bootstrap)!=32768:raise ValueError('bootstrap vượt 32KiB')
        header=bytearray(64);struct.pack_into('<8sHH7I',header,0,b'LAMIXS2\0',1,64,64,len(core),memory,0x20000,symbols['stage2_core_entry']-0x20000,zlib.crc32(core),0)
        struct.pack_into('<I',header,36,zlib.crc32(header));bootstrap[-64:]=header
        (output/'stage2.bin').write_bytes(bootstrap+core)
        run('nasm','--reproducible','-f','elf64','-o',output/'probe-entry.o','tests/bios_stage2/probe.asm')
        run(x['gcc'],*flags,'-m64','-mcmodel=kernel','-mno-red-zone','-c','lib/boot/contract.c','-o',output/'probe-contract.o')
        for fault,name in [(0,'kernel'),(1,'kernel-ro-fault'),(2,'kernel-guard-fault')]:
            run(x['gcc'],*flags,'-m64','-mcmodel=kernel','-mno-red-zone',f'-DPROBE_FAULT={fault}','-c','tests/bios_stage2/probe.c','-o',output/(name+'.o'))
            elf=output/(name+'.elf')
            run(x['ld'],'--fatal-warnings','--build-id=none','-z','noexecstack','-z','max-page-size=0x1000','-T',ROOT/'tests/bios_stage2/probe.ld',
                '-o',elf,output/'probe-entry.o',output/'probe-contract.o',output/(name+'.o'))
            data=bytearray(elf.read_bytes());phoff=struct.unpack_from('<Q',data,32)[0];stride,count=struct.unpack_from('<HH',data,54)
            for i in range(count):
                at=phoff+i*stride
                if struct.unpack_from('<I',data,at)[0]==1:struct.pack_into('<Q',data,at+24,0)
            elf.write_bytes(data);inspect_elf(elf,64)
        for name in ('core.elf','kernel.elf','kernel-ro-fault.elf','kernel-guard-fault.elf'):
            if subprocess.check_output([str(x['nm']),'-u',str(output/name)],text=True).strip():raise ValueError('undefined '+name)
        run('nasm','--reproducible','-f','bin','-o',output/'stage1.bin','boot/bios/stage1.asm')
        if hashlib.sha256((output/'stage1.bin').read_bytes()).hexdigest()!=STAGE1_HASH:raise ValueError('Stage1 thay đổi')
    source_files=[* (ROOT/'boot/bios/stage2').glob('*'),*(ROOT/'include/lamix/boot').glob('*.h'),*(ROOT/'lib/boot').glob('*.c'),ROOT/'tests/bios_stage2/probe.c',ROOT/'tests/bios_stage2/probe.asm',ROOT/'tests/bios_stage2/probe.ld',Path(__file__).resolve(),ROOT/'scripts/toolchain.py',ROOT/'tests/bios_stage2/build_gate.py']
    report=dict(source_hashes={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in source_files if p.is_file()},status='PASS',mode='build-only',profile=profile,test_fault=test_fault,m16_video=m16_video,core_bytes=len(core),core_memory=memory,bootstrap_bytes=32768,
                hashes={n:hashlib.sha256((output/n).read_bytes()).hexdigest() for n in ('stage1.bin','stage2.bin','kernel.elf')},commands=commands)
    (output/'build.json').write_text(json.dumps(report,indent=2)+'\n')
    print('PASS: build-only',len(core),'file bytes,',memory,'core memory; chưa phải boot PASS')
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--build',type=Path,required=True);p.add_argument('--profile',choices=['Debug','Release'],default='Debug');p.add_argument('--test-fault',choices=['lbp','map']);p.add_argument('--m16-video',action='store_true');a=p.parse_args();build(a.build,a.profile,a.test_fault,a.m16_video)
