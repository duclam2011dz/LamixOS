# SPDX-License-Identifier: GPL-3.0-only
"""Build provenance và guest ACPI/APIC M14 qua BIOS/UEFI."""
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

def build_kernel(directory):
    subprocess.run([sys.executable,str(ROOT/"tests/m14/build_kernel.py"),"--build",str(directory)],check=True,timeout=240,capture_output=True,text=True)
    return directory

def digest(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def platform_artifacts():
    bios=Path(os.environ.get("LAMIX_STAGE2_BUILD",ROOT/"build/m14-bios"))
    uefi=Path(os.environ.get("LAMIX_UEFI_BUILD",ROOT/"build/m14-uefi"))
    if not (bios/"build.json").is_file():
        subprocess.run([sys.executable,str(ROOT/"tests/bios_stage2/build_stage2.py"),"--build",str(bios),"--profile","Release"],check=True,timeout=240)
    if not (uefi/"build.json").is_file():
        subprocess.run([sys.executable,str(ROOT/"tests/uefi/build_uefi.py"),"--build",str(uefi)],check=True,timeout=240)
    return bios,uefi

def test_m14_kernel_provenance():
    directory=build_kernel(ROOT/"build/m14-host")
    report=json.loads((directory/"build.json").read_text(encoding="utf-8"))
    assert report["status"]=="PASS" and report["mode"]=="m14-kernel"
    assert report["hashes"]["kernel.elf"]==digest(directory/"kernel.elf")
    for source in ("kernel/acpi.c","kernel/apic.c","kernel/apic.h","tests/m14/build_kernel.py"):
        assert source in report["source_hashes"]
    inspection=(directory/"kernel.inspection.txt").read_text(encoding="utf-8")
    assert "description data" in inspection and "02 00 00 00" in inspection

@pytest.fixture(scope="session")
def m14_image(tmp_path_factory):
    bios,uefi=platform_artifacts()
    kernel=build_kernel(ROOT/"build/m14-guest")
    image=tmp_path_factory.mktemp("m14-image")/"shared.img"
    created=create_image(image,bios,uefi,kernel_build=kernel,seed="m14-apic-ioapic-lapic-timer")
    checked=check_image(image,bios,uefi,kernel_build=kernel)
    assert created["kernel_sha256"]==checked["kernel_sha256"]
    return image,bios,uefi,kernel,created

@pytest.mark.qemu
@pytest.mark.parametrize("profile,purpose",[("bios","bios-stage2"),("uefi","uefi-loader")])
def test_m14_guest_apic_timer_three_boots(m14_image,profile,purpose):
    image,bios,uefi,kernel,build_report=m14_image
    reports=[]
    for attempt in range(1,4):
        before=image_fingerprints(image,bios,uefi,kernel)
        result=run(Config(profile=profile,purpose=purpose,image=image,success_marker="M14:PASS",panic_marker="M14:FAIL",timeout=75,output_root=ROOT/"out/m14"))
        after=image_fingerprints(image,bios,uefi,kernel)
        serial=(Path(result.directory)/"serial.log").read_text(errors="replace")
        reports.append({"attempt":attempt,"status":result.status,"detail":result.detail,"serial":serial,"qemu":result.__dict__,"protected_before":before,"protected_after":after})
        assert before==after, "QEMU đã sửa image, firmware hoặc kernel đầu vào"
        assert not result.cleanup_forced and result.cleanup_error is None
        assert result.status=="PASS", result.detail
        for marker in ("M08:PASS\n","M09:PASS\n","M10:PASS\n","M11:PASS\n","M12:PASS\n","M13:PASS\n","M14:ACPI:PASS\n","M14:APIC:PASS\n","M14:IOAPIC:PASS\n","M14:PIT_REF:PASS\n","M14:LAPIC_TIMER:PASS\n","M14:PASS\n"):
            assert marker in serial, f"{marker!r} thiếu trong lần boot {attempt}"
        assert "M14:FAIL" not in serial and "M08:PANIC" not in serial
        assert serial.count("M14:PIT_REF:PASS") == 1 and serial.count("M14:LAPIC_TIMER:PASS") == 1
    report={"status":"PASS","profile":profile,"kernel_sha256":build_report["kernel_sha256"],"boots":reports}
    destination=ROOT/"out/m14/results"; destination.mkdir(parents=True,exist_ok=True)
    (destination/f"{profile}.json").write_text(json.dumps(report,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
