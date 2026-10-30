#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Build ELF64 kernel M08 và provenance; chỉ ghi dưới build/."""
import argparse, hashlib, json, shutil, struct, subprocess
from pathlib import Path
import sys
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT))
from scripts.toolchain import cross_tools

SOURCES=("kernel/entry.asm","kernel/interrupts.asm","kernel/kernel.c","kernel/linker.ld",
         "include/lamix/boot/lbp.h","include/lamix/boot/contract.h",
         "lib/boot/contract.c","lib/boot/memory.c","tests/m08/build_kernel.py")
def sha(p): return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def build(output, fault="none", m09_pmm=False, m10_vmm=False):
    if m10_vmm:
        m09_pmm = True
    output=Path(output).resolve(strict=False)
    if not output.is_relative_to(ROOT/"build"): raise ValueError("output ngoài build/")
    output.mkdir(parents=True,exist_ok=True)
    (output/"build.json").write_text(json.dumps({"status":"FAIL","mode":"m08-kernel","detail":"build chưa hoàn tất"})+"\n")
    x=cross_tools(); commands=[]
    def run(*args):
        args=list(map(str,args));commands.append(args);subprocess.run(args,cwd=ROOT,check=True,timeout=90)
    variants={"none":None,"ud":"LAMIX_FAULT_UD","pf":"LAMIX_FAULT_PF"}
    m10_variants={"none":None,"ro":"LAMIX_M10_FAULT_RO",
                  "nx":"LAMIX_M10_FAULT_NX","guard":"LAMIX_M10_FAULT_GUARD"}
    if m10_vmm:
        if fault not in m10_variants: raise ValueError("fault M10 không hợp lệ")
    elif fault not in variants:
        raise ValueError("fault không hợp lệ")
    sources=list(SOURCES)
    if m09_pmm:
        sources.extend(("kernel/pmm.c","kernel/pmm.h","tests/m09/build_kernel.py"))
    if m10_vmm:
        sources.extend(("kernel/vmm.c","kernel/vmm.h","tests/m10/build_kernel.py"))
    common=[x["gcc"],"-std=c11","-m64","-mcmodel=kernel","-mno-red-zone","-mgeneral-regs-only","-ffreestanding","-fno-builtin","-fno-pie","-fno-pic","-fno-stack-protector","-fno-asynchronous-unwind-tables","-fno-unwind-tables","-fno-exceptions","-Wall","-Wextra","-Werror","-I",ROOT/"include","-I",ROOT/"kernel","-O2"]
    run("nasm","--reproducible","-f","elf64","-o",output/"entry.o",ROOT/"kernel/entry.asm")
    run("nasm","--reproducible","-f","elf64","-o",output/"interrupts.o",ROOT/"kernel/interrupts.asm")
    objects=[]
    compile_sources=["kernel/kernel.c","lib/boot/contract.c","lib/boot/memory.c"]
    if m09_pmm:
        compile_sources.append("kernel/pmm.c")
    if m10_vmm:
        compile_sources.append("kernel/vmm.c")
    for source in compile_sources:
        obj=output/(Path(source).stem+".o"); args=common.copy()
        if source=="kernel/kernel.c" and not m10_vmm and variants[fault]: args += ["-D"+variants[fault]+"=1"]
        if source=="kernel/kernel.c" and m09_pmm: args += ["-DLAMIX_M09_PMM=1"]
        if source=="kernel/kernel.c" and m10_vmm:
            args += ["-DLAMIX_M10_VMM=1"]
            if m10_variants[fault]: args += ["-D"+m10_variants[fault]+"=1"]
        run(*args,"-c",ROOT/source,"-o",obj);objects.append(obj)
    elf=output/"kernel.elf"
    run(x["ld"],"--fatal-warnings","--build-id=none","-z","noexecstack","-z","max-page-size=0x1000","-T",ROOT/"kernel/linker.ld","-Map",output/(elf.stem+".map"),"-o",elf,output/"entry.o",output/"interrupts.o",*objects)
    raw=bytearray(elf.read_bytes())
    if raw[:6]!=b"\x7fELF\x02\x01" or struct.unpack_from("<HH",raw,16)!=(2,62): raise ValueError("ELF64 ET_EXEC x86_64 little-endian không hợp lệ")
    if struct.unpack_from("<Q",raw,24)[0] < 0xffffffff80000000: raise ValueError("entry không nằm higher-half")
    phoff=struct.unpack_from("<Q",raw,32)[0]; stride,count=struct.unpack_from("<HH",raw,54)
    if stride<56 or not count: raise ValueError("ELF program headers không hợp lệ")
    loads=0
    for i in range(count):
        at=phoff+i*stride; typ=struct.unpack_from("<I",raw,at)[0]
        if typ==1:
            loads+=1
            flags=struct.unpack_from("<I",raw,at+4)[0]; offset,vaddr,_,filesz,memsz=struct.unpack_from("<QQQQQ",raw,at+8)
            if vaddr<0xffffffff80000000 or filesz>memsz or offset+filesz>len(raw) or flags&3==3: raise ValueError("PT_LOAD bounds/permissions không hợp lệ")
            struct.pack_into("<Q",raw,at+24,0)
        elif typ in (2,3): raise ValueError("ELF dynamic/interpreter program header không được phép")
    if loads<2: raise ValueError("kernel cần các load segments tách quyền")
    elf.write_bytes(raw)
    inspect=subprocess.check_output([str(x["readelf"]),"-h","-l","-n",str(elf)],text=True)
    (output/(elf.stem+".inspection.txt")).write_text(inspect)
    undefined=subprocess.check_output([str(x["nm"]),"-u",str(elf)],text=True).strip()
    if undefined: raise ValueError("undefined symbols: "+undefined)
    if b"INTERP" in subprocess.check_output([str(x["readelf"]),"-l",str(elf)]): raise ValueError("PT_INTERP không freestanding")
    report={"status":"PASS","mode":"m10-kernel" if m10_vmm else "m09-kernel" if m09_pmm else "m08-kernel","fault":fault,"artifact":elf.name,"entry":"kernel_entry","commands":commands,
            "tool_versions":{"gcc":subprocess.check_output([str(x["gcc"]),"--version"],text=True).splitlines()[0],"ld":subprocess.check_output([str(x["ld"]),"--version"],text=True).splitlines()[0],"nasm":subprocess.check_output(["nasm","-v"],text=True).strip()},
            "source_hashes":{s:sha(ROOT/s) for s in sources},"hashes":{elf.name:sha(elf)},"elf_inspection":elf.stem+".inspection.txt","load_segments":loads}
    (output/"build.json").write_text(json.dumps(report,ensure_ascii=False,indent=2)+"\n")
    print(json.dumps(report,ensure_ascii=False,indent=2))
def main():
    p=argparse.ArgumentParser();p.add_argument("--build",type=Path,required=True);p.add_argument("--fault",choices=("none","ud","pf"),default="none");a=p.parse_args();build(a.build,a.fault)
if __name__=="__main__":main()
