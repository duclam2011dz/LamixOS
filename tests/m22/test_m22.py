# SPDX-License-Identifier: GPL-3.0-only
"""M22 profile build provenance and ring-3 capability/channel integration."""
import hashlib
from pathlib import Path
import subprocess
import sys
import pytest
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT))
from scripts.m07_image import check_image,create_image,verify_build_report
from scripts.qemu_harness import Config,run
sys.path.insert(0,str(ROOT/"tests/m17"))
from test_m17 import platform_artifacts
sys.path.insert(0,str(ROOT/"tests/m10"))
from test_m10 import image_fingerprints
def digest(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def build_m22():
    output=ROOT/"build/m22-guest"
    subprocess.run([sys.executable,str(ROOT/"tests/m22/build_kernel.py"),"--build",str(output)],check=True,timeout=300,capture_output=True,text=True)
    return output
def test_m22_build_provenance():
    output=build_m22();report=verify_build_report(output,("kernel.elf",))
    assert report["status"]=="PASS" and report["mode"]=="m22-kernel"
    assert report["hashes"]["kernel.elf"]==digest(output/"kernel.elf")
    for source in ("kernel/handle.c","kernel/ipc.c","kernel/syscall.c","user/syscall.c","tests/m22/user_payload.c"):
        assert source in report["source_hashes"]
@pytest.fixture(scope="session")
def artifacts():
    bios,uefi=platform_artifacts();kernel=build_m22();report=verify_build_report(kernel,("kernel.elf",))
    br=verify_build_report(bios,("stage1.bin","stage2.bin","kernel.elf"));ur=verify_build_report(uefi,("BOOTX64.EFI",))
    image=ROOT/"out/m22"/(f"lamix-m22-{report['hashes']['kernel.elf'][:12]}-{br['hashes']['stage2.bin'][:8]}-{ur['hashes']['BOOTX64.EFI'][:8]}.img")
    image.parent.mkdir(parents=True,exist_ok=True)
    created=check_image(image,bios,uefi,kernel_build=kernel) if image.exists() else create_image(image,bios,uefi,kernel_build=kernel,seed="lamix-m22-ipc")
    assert created["kernel_sha256"]==check_image(image,bios,uefi,kernel_build=kernel)["kernel_sha256"]
    return image,bios,uefi,kernel,report
@pytest.mark.qemu
@pytest.mark.parametrize("profile,purpose",[("bios","bios-stage2"),("uefi","uefi-loader")])
@pytest.mark.parametrize("attempt",range(1,4))
def test_m22_ring3_channel_guest(artifacts,profile,purpose,attempt):
    image,bios,uefi,kernel,report=artifacts;before=image_fingerprints(image,bios,uefi,kernel)
    result=run(Config(profile=profile,purpose=purpose,image=image,success_marker="M22:PASS",panic_marker="M22:FAIL",timeout=180,output_root=ROOT/"out/m22/runs",extra_args=["-vga","none","-device","virtio-vga"]))
    serial=(Path(result.directory)/"serial.log").read_text(errors="replace");after=image_fingerprints(image,bios,uefi,kernel)
    assert before==after and result.status=="PASS" and not result.cleanup_forced and result.cleanup_error is None
    for marker in ("M20:RING3:ENTERING\n","M22:CROSS_PROCESS:PASS\n","M22:USER:ABI:PASS\n","M22:CAPABILITY_CHANNEL:PASS\n","M22:FAULT_ISOLATION:PASS\n","M22:PASS\n"):
        assert marker in serial,marker
