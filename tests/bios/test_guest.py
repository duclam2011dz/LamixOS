# SPDX-License-Identifier: GPL-3.0-only
"""QEMU/SeaBIOS thực thi Stage1 và probe; không phải OS boot."""
import hashlib
import json
import os
from pathlib import Path

import pytest
from bios_support import ROOT, stage1_artifact
from image_fixture import metadata, assemble_probe, create_image, extent_hashes, HEADER_MUTATIONS, PARTITION_MUTATIONS
from scripts.qemu_harness import Config, run

CASES=[('valid','PASS',None),('relocated','PASS',None),('exact-window','PASS',None),('payload','PANIC','P'),('trailer','PANIC','P'),
       ('missing','PANIC','N'),('io-error','PANIC','R')]
CASES += [(name,'PANIC','G') for name in HEADER_MUTATIONS]
CASES += [(name,'PANIC','G') for name in PARTITION_MUTATIONS]
CASES += [(f'guid-{i}','PANIC','N') for i in range(16)]
CASES += [(f'header-byte-{i}','PANIC','G') for i in list(range(16))+list(range(72,88))]

@pytest.mark.qemu
@pytest.mark.parametrize('variant,expected,reason',CASES)
def test_real_bios_stage1(tmp_path,stage1_artifact,variant,expected,reason):
    kind='valid' if variant=='io-error' else variant
    _,start,end=metadata(kind)
    probe=assemble_probe(tmp_path/'probe.bin',start,end)
    image=tmp_path/'fixture.img'
    manifest=create_image(image,stage1_artifact.read_bytes(),probe,kind)
    before=extent_hashes(image,manifest)
    firmware=Path('/usr/share/seabios/bios.bin')
    firmware_before=hashlib.sha256(firmware.read_bytes()).digest()
    result=run(Config(profile='bios',purpose='bios-stage1',image=image,
                      test_io_error_sector=start+31 if variant=='io-error' else None,
                      success_marker='M04:PASS',panic_marker='M04:FAIL',timeout=10,
                      output_root=ROOT/'out/m04'))
    serial=(Path(result.directory)/'serial.log').read_text().splitlines()
    after=extent_hashes(image,manifest)
    firmware_after=hashlib.sha256(firmware.read_bytes()).digest()
    (Path(result.directory)/'case.json').write_text(json.dumps({
        'variant':variant,'expected_status':expected,'actual_status':result.status,
        'expected_reason':reason,'serial_lines':serial,'backend':result.backend,'mode':result.mode,
        'protected_extents_before':before,'protected_extents_after':after,
        'firmware_unchanged':firmware_after==firmware_before,
        'partition_start':start,'partition_end':end},indent=2)+'\n')
    assert result.status==expected, (result,serial)
    assert result.backend=='qemu' and result.mode=='bios-stage1'
    assert result.returncode==0 and not result.cleanup_forced and not result.cleanup_error
    assert '-S' not in result.command and 'snapshot=on' in ' '.join(result.command)
    if reason:
        assert serial==['M04:FAIL',reason] and 'M04:PASS' not in serial
    else:
        assert serial==['M04:PASS']
    if variant=='io-error':
        assert 'file.driver=blkdebug' in ' '.join(result.command)
        assert f'sector = "{start+31}"' in (Path(result.directory)/'blkdebug.conf').read_text()
    assert after==before
    assert firmware_after==firmware_before
    with pytest.raises(ChildProcessError):os.waitpid(result.pid,os.WNOHANG)
    with pytest.raises(ProcessLookupError):os.killpg(result.pid,0)
