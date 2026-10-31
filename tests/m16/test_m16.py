# SPDX-License-Identifier: GPL-3.0-only
"""M16 renderer provenance and firmware guest checks."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import pytest
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT))
from scripts.m07_image import create_image, check_image, verify_build_report
from scripts.qemu_harness import Config, run
sys.path.insert(0,str(ROOT/"tests/m10"))
from test_m10 import image_fingerprints

def digest(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def build_kernel(directory):
    subprocess.run([sys.executable,str(ROOT/"tests/m16/build_kernel.py"),"--build",str(directory)],check=True,timeout=300,capture_output=True,text=True)
    return directory

def test_m16_kernel_provenance():
    directory=build_kernel(ROOT/"build/m16-host")
    report=json.loads((directory/"build.json").read_text(encoding="utf-8"))
    assert report["status"]=="PASS" and report["mode"]=="m16-kernel"
    assert report["hashes"]["kernel.elf"]==digest(directory/"kernel.elf")
    assert "kernel/graphics.c" in report["source_hashes"]

def platform_artifacts():
    bios=Path(os.environ.get("LAMIX_STAGE2_BUILD",ROOT/"build/m16-bios"))
    uefi=Path(os.environ.get("LAMIX_UEFI_BUILD",ROOT/"build/m16-uefi"))
    bios_current=False
    if (bios/"build.json").is_file():
        try:
            report=json.loads((bios/"build.json").read_text(encoding="utf-8"))
            verify_build_report(bios,("stage1.bin","stage2.bin","kernel.elf"))
            bios_current=report.get("m16_video") is True
        except (OSError,ValueError,json.JSONDecodeError):
            bios_current=False
    if not bios_current:
        subprocess.run([sys.executable,str(ROOT/"tests/bios_stage2/build_stage2.py"),"--build",str(bios),"--profile","Release","--m16-video"],check=True,timeout=300)
    if not (uefi/"build.json").is_file():
        subprocess.run([sys.executable,str(ROOT/"tests/uefi/build_uefi.py"),"--build",str(uefi)],check=True,timeout=300)
    return bios,uefi

def input_script():
    def key(code,down):
        return {"execute":"input-send-event","arguments":{"events":[{"type":"key","data":{"key":{"type":"qcode","data":code},"down":down}}]}}
    return [key("a",True),key("a",False),key("shift",True),key("a",True),key("a",False),key("shift",False),
            {"execute":"input-send-event","arguments":{"events":[{"type":"rel","data":{"axis":"x","value":23}},{"type":"rel","data":{"axis":"y","value":-11}}]}},
            {"execute":"input-send-event","arguments":{"events":[{"type":"btn","data":{"down":True,"button":"left"}}]}},
            {"execute":"input-send-event","arguments":{"events":[{"type":"btn","data":{"down":False,"button":"left"}}]}}]

@pytest.fixture(scope="session")
def m16_image(tmp_path_factory):
    bios,uefi=platform_artifacts(); kernel=build_kernel(ROOT/"build/m16-guest")
    image=tmp_path_factory.mktemp("m16-image")/"shared.img"
    created=create_image(image,bios,uefi,kernel_build=kernel,seed="m16-software-renderer")
    checked=check_image(image,bios,uefi,kernel_build=kernel)
    assert created["kernel_sha256"]==checked["kernel_sha256"]
    return image,bios,uefi,kernel,created

@pytest.mark.qemu
@pytest.mark.parametrize("profile,purpose",[("bios","bios-stage2"),("uefi","uefi-loader")])
def test_m16_guest_framebuffer(m16_image,profile,purpose):
    image,bios,uefi,kernel,build_report=m16_image
    before=image_fingerprints(image,bios,uefi,kernel)
    cfg=Config(profile=profile,purpose=purpose,image=image,success_marker="M16:PASS",panic_marker="M16:FAIL",timeout=120,output_root=ROOT/"out/m16",qmp_trigger_marker="M15:READY",qmp_script=input_script())
    result=run(cfg); after=image_fingerprints(image,bios,uefi,kernel)
    serial=(Path(result.directory)/"serial.log").read_text(errors="replace")
    assert before==after, "QEMU đã sửa image/firmware/kernel đầu vào"
    assert not result.cleanup_forced and result.cleanup_error is None
    assert result.status=="PASS", result.detail
    assert "M16:RENDERER:PASS\n" in serial and "M16:FRAME:PASS checksum=" in serial
    assert "M16:PASS\n" in serial and "M16:FAIL" not in serial
    assert "M16:FRAMEBUFFER:UNAVAILABLE" not in serial
    if profile=="bios": assert "M16:VBE:MODE=1024x768\n" in serial or "M16:VBE:MODE=800x600\n" in serial
    destination=ROOT/"out/m16/results"; destination.mkdir(parents=True,exist_ok=True)
    (destination/f"{profile}.json").write_text(json.dumps({"status":"PASS","profile":profile,"kernel_sha256":build_report["kernel_sha256"],"result":result.__dict__,"serial":serial},ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
