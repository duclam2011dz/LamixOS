# SPDX-License-Identifier: GPL-3.0-only
"""Guest thật; negative expected PANIC không đổi thành guest PASS."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import pytest
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT));sys.path.insert(0,str(Path(__file__).resolve().parent))
from scripts.qemu_harness import Config,run
from stage2_fixture import create_stage2_image,hashes

@pytest.fixture(scope='session')
def stage2_build():
    build=Path(os.environ.get('LAMIX_STAGE2_BUILD',ROOT/'build/m05'))
    if not (build/'build.json').exists():
        subprocess.run([sys.executable,str(ROOT/'tests/bios_stage2/build_stage2.py'),'--build',str(build)],check=True)
    report=json.loads((build/'build.json').read_text())
    assert report['status']=='PASS','Build M05 chưa PASS; không chạy guest với artifact cũ'
    for name,digest in report['source_hashes'].items():
        assert hashlib.sha256((ROOT/name).read_bytes()).hexdigest()==digest,'Source đã đổi, phải build M05 lại: '+name
    for name,digest in report['hashes'].items():
        assert hashlib.sha256((build/name).read_bytes()).hexdigest()==digest,'Artifact M05 đã đổi: '+name
    return build

CASES=[('valid','kernel.elf',256,None,None),('valid','kernel.elf',64,None,None),('valid','kernel.elf',4608,None,None),
       ('fragmented','kernel.elf',256,None,None),('fragmented-dir','kernel.elf',256,None,None),('active-fat','kernel.elf',256,None,None),('valid','kernel.elf',256,'qemu64,-nx',None),
       ('valid','kernel-ro-fault.elf',256,None,'EXCEPTION'),('valid','kernel-guard-fault.elf',256,None,'EXCEPTION'),
       ('note-required-nx','kernel.elf',256,'qemu64,-nx','NOTE'),('valid','kernel.elf',256,'qemu64,-lm','CPU'),
       ('oom','kernel.elf',64,None,'MEMORY')]
CASES += [(v,'kernel.elf',256,None,r) for v,r in [('header-crc','S2'),('core-crc','S2'),('gpt-backup-crc','GPT'),('gpt-array-crc','GPT'),
        ('fat-bpb','FAT'),('fat-loop','FAT'),('fat-mirror','FAT'),('missing-a','FAT'),('elf-magic','ELF'),('note-duplicate','ELF'),('note-required-acpi','NOTE'),
        ('inject-e820-bad','E820'),('inject-e820-truncated','E820'),('inject-e820-overflow','E820'),('inject-a20','A20'),('io-core','DISK'),('io-fat','DISK'),('io-kernel','DISK'),('inject-lbp','LBP'),('inject-map','LBP')]]

CASES += [(v,'kernel.elf',256,None,'S2') for v in ('s2-version','s2-core-lba','s2-empty','s2-memory','s2-load','s2-entry','s2-reserved')]
CASES += [(v,'kernel.elf',256,None,'GPT') for v in ('gpt-overlap','gpt-duplicate','gpt-guid')]
CASES += [(v,'kernel.elf',256,None,'ELF') for v in ('elf-paddr','elf-wx','elf-entry','elf-overlap','elf-wrap','elf-dynamic')]
CASES += [(v,'kernel.elf',256,None,'FAT') for v in ('dir-loop','fat-free','fat-short')]

CASES += [('inject-segments','kernel.elf',256,None,None),('large-core','kernel.elf',256,None,None),('large-core-crc','kernel.elf',256,None,'S2')]

@pytest.mark.qemu
@pytest.mark.parametrize('variant,kernel,ram,cpu,reason',CASES)
def test_bios_stage2_contract(tmp_path,stage2_build,variant,kernel,ram,cpu,reason):
    image=tmp_path/'disk.img';kind='valid' if variant.startswith('io-') or variant in ('inject-lbp','inject-map') else variant
    if variant in ('inject-lbp','inject-map'):
        profile=json.loads((stage2_build/'build.json').read_text())['profile']
        fault=variant.removeprefix('inject-');stage2_build=stage2_build/('fault-'+fault)
        subprocess.run([sys.executable,str(ROOT/'tests/bios_stage2/build_stage2.py'),'--build',str(stage2_build),'--test-fault',fault,'--profile',profile],check=True)
    manifest=create_stage2_image(image,stage2_build,kind,kernel);before=hashes(image,manifest)
    if variant.startswith('large-core'):assert manifest['core_file_bytes']>65536
    firmware=Path('/usr/share/seabios/bios.bin');fw=hashlib.sha256(firmware.read_bytes()).hexdigest()
    error=manifest.get({'io-core':'core_sector','io-fat':'fat_sector','io-kernel':'kernel_sector'}.get(variant,''))
    result=run(Config(purpose='bios-stage2',image=image,success_marker='M05:PASS',panic_marker='M05:FAIL',timeout=15,output_root=ROOT/'out/m05',
                      test_io_error_sector=error,test_stage2_memory_mib=ram,test_stage2_cpu=cpu))
    serial=(Path(result.directory)/'serial.log').read_text().splitlines();after=hashes(image,manifest)
    expected='PANIC' if reason else 'PASS'
    report=dict(variant=variant,kernel=kernel,ram_mib=ram,cpu=cpu,expected=expected,actual=result.status,reason=reason,serial=serial,
                backend=result.backend,mode=result.mode,protected_before=before,protected_after=after,firmware_unchanged=fw==hashlib.sha256(firmware.read_bytes()).hexdigest(),
                cleanup_forced=result.cleanup_forced,cleanup_error=result.cleanup_error,allocated_bytes=manifest['allocated_bytes'])
    (Path(result.directory)/'case.json').write_text(json.dumps(report,indent=2)+'\n')
    assert result.status==expected,(result,serial)
    assert result.backend=='qemu' and result.mode=='bios-stage2'
    assert result.returncode==0 and not result.cleanup_forced and not result.cleanup_error
    assert '-S' not in result.command and 'snapshot=on' in ' '.join(result.command)
    assert before==after and report['firmware_unchanged']
    if reason:assert 'M05:PASS' not in serial and serial[serial.index('M05:FAIL')+1]==reason
    else:
        assert serial[-3]=='M05:HANDOFF' and serial[-1]=='M05:PASS'
        maps=[[int(x,16) for x in line.split()[1:]] for line in serial if line.startswith('M05:MAP ')]
        memory=[[int(x,16) for x in line.split()[1:]] for line in serial if line.startswith('M05:MEMORY ')]
        reserves=[[int(x,16) for x in line.split()[1:]] for line in serial if line.startswith('M05:RESERVE ')]
        assert maps and memory and reserves
        if ram==4608:assert any(base>=2**32 and length>0 for base,length,typ in memory)
        (Path(result.directory)/'layout.json').write_text(json.dumps(dict(memory=memory,reservations=reserves,mappings=maps),indent=2)+'\n')
    if reason=='EXCEPTION':assert serial[-1]=='vector=0e'
    with pytest.raises(ChildProcessError):os.waitpid(result.pid,os.WNOHANG)
    with pytest.raises(ProcessLookupError):os.killpg(result.pid,0)
