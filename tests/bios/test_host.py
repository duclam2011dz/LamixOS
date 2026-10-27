# SPDX-License-Identifier: GPL-3.0-only
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess
import sys

import pytest
from bios_support import ROOT, stage1_artifact
from image_fixture import metadata, assemble_probe, create_image, extent_hashes
from scripts.qemu_harness import Config, build_command, run


def test_binary_gate_and_report(stage1_artifact):
    binary = stage1_artifact.read_bytes()
    listing = stage1_artifact.with_suffix('.lst').read_text()
    match = re.search(r'^\s*\d+\s+([0-9A-F]{8})\s+.*times 440',listing,re.M)
    assert match
    used = int(match[1],16)
    assert 0 < used <= 440 and len(binary)==440 and not any(binary[used:])
    assert binary[0]==0xea and binary[3:5]==b'\0\0'
    assert b'EFI PART'+struct.pack('<IIQII',0x10000,92,2,128,128) in binary
    assert bytes.fromhex('4861682149646f6e744e656564454649') in binary
    report={'status':'PASS','used_bytes':used,'binary_bytes':len(binary),'padding_bytes':440-used,
            'sha256':hashlib.sha256(binary).hexdigest(),'mode':'binary-layout-only'}
    (ROOT/'out/m04').mkdir(parents=True,exist_ok=True)
    (ROOT/'out/m04/binary.json').write_text(json.dumps(report,indent=2)+'\n')


def test_budget_cannot_be_bypassed(tmp_path):
    source=(ROOT/'boot/bios/stage1.asm').read_text()
    source=source.replace('stage1_used equ','times 440 db 0\nstage1_used equ')
    path=tmp_path/'oversize.asm';path.write_text(source)
    result=subprocess.run(['nasm','-f','bin','-o',str(tmp_path/'bad.bin'),str(path)],capture_output=True,text=True)
    assert result.returncode!=0 and 'TIMES value' in result.stderr


def test_binary_reproducible_two_roots(tmp_path,stage1_artifact):
    outputs=[]
    for name in ('source-a','different-root-b'):
        root=tmp_path/name;root.mkdir()
        source=root/'stage1.asm';source.write_bytes((ROOT/'boot/bios/stage1.asm').read_bytes())
        output=root/'stage1.bin'
        subprocess.run(['nasm','--reproducible','-f','bin','-o',str(output),str(source)],check=True)
        outputs.append(output.read_bytes())
    assert outputs[0]==outputs[1]==stage1_artifact.read_bytes()


def test_sparse_fixture_and_guards(tmp_path,stage1_artifact):
    _,start,end=metadata()
    probe=assemble_probe(tmp_path/'probe.bin',start,end)
    image=tmp_path/'fixture.img'
    manifest=create_image(image,stage1_artifact.read_bytes(),probe)
    assert image.stat().st_size==8*1024**3 and image.stat().st_blocks*512<1024**2
    assert probe[-16:]==b'M04:END:32K!!!!!'
    before=extent_hashes(image,manifest)
    with pytest.raises(FileExistsError):create_image(image,stage1_artifact.read_bytes(),probe)
    assert extent_hashes(image,manifest)==before
    for bad in (b'x'*439,b'x'*441):
        with pytest.raises(ValueError):create_image(tmp_path/'bad.img',bad,probe)
    with pytest.raises(ValueError):create_image('/dev/null',stage1_artifact.read_bytes(),probe)
    link=tmp_path/'link.img';link.symlink_to(image)
    with pytest.raises(FileExistsError):create_image(link,stage1_artifact.read_bytes(),probe)
    assert extent_hashes(image,manifest)==before

@pytest.mark.parametrize('changes',[{'profile':'uefi'},{'launch_only':True},{'image':None},
                                    {'purpose':'other'},{'purpose':'os','test_io_error_sector':0}])
def test_stage1_configuration_rejects_wrong_mode(tmp_path,changes):
    image=tmp_path/'disk.img';image.write_bytes(bytes(512))
    config=dict(profile='bios',purpose='bios-stage1',image=image,output_root=tmp_path,
                success_marker='M04:PASS',panic_marker='M04:FAIL')
    config.update(changes)
    assert run(Config(**config)).status=='LAUNCH_ERROR'

@pytest.mark.parametrize('sector',[-1,1,True,float('nan')])
def test_bad_fault_sector_is_rejected(tmp_path,sector):
    image=tmp_path/'disk.img';image.write_bytes(bytes(512))
    config=Config(profile='bios',purpose='bios-stage1',image=image,test_io_error_sector=sector)
    with pytest.raises(ValueError):build_command(config,tmp_path)


def test_cli_stage1_requires_image(tmp_path):
    import os
    env=os.environ.copy()
    for key in ('LAMIX_TEST_IMAGE','LAMIX_TEST_SUCCESS_MARKER','LAMIX_TEST_PANIC_MARKER'):
        env.pop(key,None)
    result=subprocess.run([sys.executable,str(ROOT/'scripts/run_qemu.py'),'--profile','bios',
                           '--purpose','bios-stage1','--output-root',str(tmp_path)],env=env,capture_output=True,text=True)
    assert result.returncode==1 and 'LAUNCH_ERROR' in result.stdout


def test_existing_positional_config_preserved(tmp_path):
    config=Config('bios',False,tmp_path/'image.img')
    assert config.image==tmp_path/'image.img' and config.purpose=='os'
