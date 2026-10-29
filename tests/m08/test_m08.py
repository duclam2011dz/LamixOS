# SPDX-License-Identifier: GPL-3.0-only
"""Kiểm tra layout/build report và boot kernel M08 qua BIOS/UEFI."""
import hashlib,json,os,subprocess,sys
from dataclasses import asdict
from pathlib import Path
import pytest
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT))
from scripts.m07_image import DISK_SIZE,check_image,create_image
from scripts.qemu_harness import Config,run

def run_build(directory,fault="none"):
    subprocess.run([sys.executable,str(ROOT/"tests/m08/build_kernel.py"),"--build",str(directory),"--fault",fault],check=True,timeout=120,capture_output=True,text=True)
    return directory
def artifacts():
    bios=Path(os.environ.get("LAMIX_STAGE2_BUILD",ROOT/"build/m07-input"));uefi=Path(os.environ.get("LAMIX_UEFI_BUILD",ROOT/"build/m06"))
    if not (bios/"build.json").is_file():subprocess.run([sys.executable,str(ROOT/"tests/bios_stage2/build_stage2.py"),"--build",str(bios),"--profile","Debug"],check=True,timeout=180)
    if not (uefi/"build.json").is_file():subprocess.run([sys.executable,str(ROOT/"tests/uefi/build_uefi.py"),"--build",str(uefi)],check=True,timeout=180)
    return bios,uefi
@pytest.mark.parametrize("fault",["none","ud","pf"])
def test_kernel_build_has_valid_provenance(tmp_path,fault):
    build=run_build(ROOT/"build"/f"m08-host-{fault}",fault)
    report=json.loads((build/"build.json").read_text())
    elf=build/"kernel.elf"
    assert report["status"]=="PASS" and report["hashes"]["kernel.elf"]==hashlib.sha256(elf.read_bytes()).hexdigest()
    data=elf.read_bytes();assert data[:6]==b"\x7fELF\x02\x01"
    assert "There are no relocations" in (build/"kernel.inspection.txt").read_text() or "Relocation section" not in (build/"kernel.inspection.txt").read_text()
    assert report["load_segments"]>=2 and report["source_hashes"]
    src=(ROOT/"kernel/kernel.c").read_text()
    asm=(ROOT/"kernel/interrupts.asm").read_text()
    assert "offsetof(interrupt_frame, vector)==120" in src and "offsetof(interrupt_frame, rip)==136" in src
    assert "gdt[63]=b>>32" in src and "tss.ist1" in src and "KTSS" in src
    assert 'out8(PIC1+1,0x20)' in src and 'out8(PIC2+1,0x28)' in src
    assert '1193182/100' in src and 'out8(PIC1,0x20)' in src
    assert "%if i=8 || i=10 || i=11 || i=12 || i=13 || i=14 || i=17 || i=21 || i=29 || i=30" in asm
    assert asm.count("%rep 256")==2 and "section .rodata\ninterrupt_vectors:" in asm and "section .text\ninterrupt_common:" in asm

@pytest.fixture(scope="session")
def m08_images(tmp_path_factory):
    bios,uefi=artifacts();root=tmp_path_factory.mktemp("m08-images");images={}
    for fault in ("none","ud","pf"):
        kb=run_build(ROOT/"build"/f"m08-guest-{fault}",fault)
        image=root/(fault+".img")
        report=create_image(image,bios,uefi,kernel_build=kb,seed="m08-"+fault)
        checked=check_image(image,bios,uefi,kernel_build=kb)
        assert report["kernel_sha256"]==checked["kernel_sha256"]==json.loads((kb/"build.json").read_text())["hashes"]["kernel.elf"]
        images[fault]=(image,bios,uefi,kb,report)
    return images

def protected_hashes(image,bios,uefi,kernel_build):
    def digest(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
    ranges=((0,512),(512,512+512+16384),(1024*1024,3*1024*1024),(3*1024**2,3*1024**2+256*1024**2),(DISK_SIZE-33*512,DISK_SIZE))
    ext=[]
    with Path(image).open("rb") as stream:
        for start,end in ranges:
            stream.seek(start);h=hashlib.sha256();left=end-start
            while left:
                b=stream.read(min(left,1024*1024));assert b;h.update(b);left-=len(b)
            ext.append(h.hexdigest())
    firmware={"seabios":digest("/usr/share/seabios/bios.bin"),"ovmf_code":digest("/usr/share/OVMF/OVMF_CODE.fd"),"ovmf_vars_template":digest("/usr/share/OVMF/OVMF_VARS.fd")}
    artifacts={"stage1":digest(bios/"stage1.bin"),"stage2":digest(bios/"stage2.bin"),"efi":digest(uefi/"BOOTX64.EFI"),"kernel":digest(kernel_build/"kernel.elf")}
    return {"image_extents":ext,"firmware":firmware,"artifacts":artifacts}

@pytest.mark.qemu
@pytest.mark.parametrize("fault,expected",[("none","M08:PASS"),("ud","vector=0000000000000006"),("pf","vector=000000000000000e")])
@pytest.mark.parametrize("profile",["bios","uefi"])
def test_m08_kernel_guest(m08_images,fault,expected,profile):
    image,bios,uefi,kb,report=m08_images[fault]
    before=protected_hashes(image,bios,uefi,kb)
    result=run(Config(profile=profile,purpose="os",image=image,success_marker="M08:PASS",panic_marker="M08:PANIC",timeout=25,output_root=ROOT/"out/m08"))
    after=protected_hashes(image,bios,uefi,kb)
    record={"profile":profile,"fault":fault,"expected":expected,"status":result.status,"detail":result.detail,
            "kernel_sha256":report["kernel_sha256"],"protected_hashes_before":before,"protected_hashes_after":after,
            "tool_versions":{"qemu":subprocess.check_output(["qemu-system-x86_64","--version"],text=True).splitlines()[0]},
            "qemu":asdict(result),"serial_report":str(Path(result.directory)/"serial.log")}
    report_dir=ROOT/"out/m08/results";report_dir.mkdir(parents=True,exist_ok=True)
    (report_dir/f"{fault}-{profile}.json").write_text(json.dumps(record,ensure_ascii=False,indent=2)+"\n")
    assert after==before,"QEMU đã sửa source image/firmware/artifacts"
    assert not result.cleanup_forced and result.cleanup_error is None,"QEMU cleanup không sạch"
    serial=Path(result.directory)/"serial.log"
    text=serial.read_text(errors="replace")
    if fault=="none":
        assert result.status=="PASS",result.detail
        assert text.count("M08:TICK\n")>=3,"PIT IRQ0 chưa được nhận ít nhất ba lần"
    else:
        assert result.status=="PANIC",result.detail
        assert "M08:PANIC" in text and expected in text
        if fault=="pf":
            guard=next(line.split("=",1)[1] for line in text.splitlines() if line.startswith("M08:GUARD="))
            panic_line=next(line for line in text.splitlines() if line.startswith("vector="))
            cr2=next(part.split("=",1)[1] for part in panic_line.split() if part.startswith("cr2="))
            assert int(cr2,16)==int(guard,16),"#PF không nhắm guard page từ LBP"
