# SPDX-License-Identifier: GPL-3.0-only
"""Host decoder/provenance and guest PS/2 keyboard/mouse acceptance tests."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import pytest
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT))
from scripts.m07_image import create_image, check_image
from scripts.qemu_harness import Config, run
sys.path.insert(0,str(ROOT/"tests/m10"))
from test_m10 import image_fingerprints

def digest(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def build_kernel(directory):
    subprocess.run([sys.executable,str(ROOT/"tests/m15/build_kernel.py"),"--build",str(directory)],check=True,timeout=240,capture_output=True,text=True)
    return directory
def test_m15_kernel_provenance():
    directory=build_kernel(ROOT/"build/m15-host")
    report=json.loads((directory/"build.json").read_text(encoding="utf-8"))
    assert report["status"]=="PASS" and report["mode"]=="m15-kernel"
    assert report["hashes"]["kernel.elf"]==digest(directory/"kernel.elf")
    for source in ("kernel/input.c","kernel/input.h","kernel/ps2.c","kernel/ps2.h","tests/m15/build_kernel.py"):
        assert source in report["source_hashes"]

def platform_artifacts():
    bios=Path(os.environ.get("LAMIX_STAGE2_BUILD",ROOT/"build/m15-bios"))
    uefi=Path(os.environ.get("LAMIX_UEFI_BUILD",ROOT/"build/m15-uefi"))
    if not (bios/"build.json").is_file():
        subprocess.run([sys.executable,str(ROOT/"tests/bios_stage2/build_stage2.py"),"--build",str(bios),"--profile","Release"],check=True,timeout=240)
    if not (uefi/"build.json").is_file():
        subprocess.run([sys.executable,str(ROOT/"tests/uefi/build_uefi.py"),"--build",str(uefi)],check=True,timeout=240)
    return bios,uefi

def input_script():
    def key(code,down):
        return {"execute":"input-send-event","arguments":{"events":[{"type":"key","data":{"key":{"type":"qcode","data":code},"down":down}}]}}
    return [key("a",True),key("a",False),key("shift",True),key("a",True),key("a",False),key("shift",False),
            {"execute":"input-send-event","arguments":{"events":[{"type":"rel","data":{"axis":"x","value":23}},{"type":"rel","data":{"axis":"y","value":-11}}]}},
            {"execute":"input-send-event","arguments":{"events":[{"type":"btn","data":{"down":True,"button":"left"}}]}},
            {"execute":"input-send-event","arguments":{"events":[{"type":"btn","data":{"down":False,"button":"left"}}]}}]

@pytest.fixture(scope="session")
def m15_image(tmp_path_factory):
    bios,uefi=platform_artifacts(); kernel=build_kernel(ROOT/"build/m15-guest")
    image=tmp_path_factory.mktemp("m15-image")/"shared.img"
    created=create_image(image,bios,uefi,kernel_build=kernel,seed="m15-ps2-keyboard-mouse")
    checked=check_image(image,bios,uefi,kernel_build=kernel)
    assert created["kernel_sha256"]==checked["kernel_sha256"]
    return image,bios,uefi,kernel,created

@pytest.mark.qemu
@pytest.mark.parametrize("profile,purpose",[("bios","bios-stage2"),("uefi","uefi-loader")])
def test_m15_keyboard_mouse_guest_three_boots(m15_image,profile,purpose):
    image,bios,uefi,kernel,build_report=m15_image; reports=[]
    for attempt in range(1,4):
        before=image_fingerprints(image,bios,uefi,kernel)
        cfg=Config(profile=profile,purpose=purpose,image=image,success_marker="M15:PASS",panic_marker="M15:FAIL",timeout=90,output_root=ROOT/"out/m15",qmp_trigger_marker="M15:READY",qmp_script=input_script())
        result=run(cfg); after=image_fingerprints(image,bios,uefi,kernel)
        serial=(Path(result.directory)/"serial.log").read_text(errors="replace")
        reports.append({"attempt":attempt,"status":result.status,"detail":result.detail,"serial":serial,"qemu":result.__dict__,"protected_before":before,"protected_after":after})
        assert before==after, "QEMU đã sửa image/firmware/kernel đầu vào"
        assert not result.cleanup_forced and result.cleanup_error is None
        assert result.status=="PASS", result.detail
        for marker in ("M08:PASS\n","M09:PASS\n","M10:PASS\n","M11:PASS\n","M12:PASS\n","M13:PASS\n","M14:PASS\n","M15:READY\n","M15:KEYBOARD:PASS\n","M15:MOUSE:PASS\n","M15:QUEUE:PASS\n","M15:PASS\n"):
            assert marker in serial, f"{marker!r} thiếu trong lần boot {attempt}"
        assert "M15:FAIL" not in serial and "M15:MOUSE:UNAVAILABLE" not in serial
    destination=ROOT/"out/m15/results"; destination.mkdir(parents=True,exist_ok=True)
    (destination/f"{profile}.json").write_text(json.dumps({"status":"PASS","profile":profile,"kernel_sha256":build_report["kernel_sha256"],"boots":reports},ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
