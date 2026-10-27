# SPDX-License-Identifier: GPL-3.0-only
import os
from pathlib import Path
import subprocess
import pytest

ROOT = Path(__file__).resolve().parents[2]

@pytest.fixture(scope='session')
def stage1_artifact(tmp_path_factory):
    override = os.environ.get('LAMIX_STAGE1_BIN')
    if override:
        path = Path(override)
        assert path.is_file(), 'Thiếu artifact Stage1 được chỉ định'
    else:
        path = tmp_path_factory.mktemp('m04-assemble')/'stage1.bin'
        subprocess.run(['nasm','--reproducible','-f','bin','-l',str(path.with_suffix('.lst')),
                        '-o',str(path),str(ROOT/'boot/bios/stage1.asm')],check=True)
    return path
