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
def build(output, fault="none", m09_pmm=False, m10_vmm=False,
          m11_heap=False, m12_buddy=False, m13_devices=False, m14_apic=False,
          m15_input=False, m16_graphics=False, m17_virtio_gpu=False,
          m19_scheduler=False, m20_process=False, m21_syscall=False,
          m22_ipc=False, m23_elf=False, m24_smp=False, max_cpus=16,
          m24_fault="none"):
    if m24_smp:
        if not 1 <= max_cpus <= 16:
            raise ValueError("M24 max_cpus must be in 1..16")
        if m24_fault not in ("none", "ap-timeout", "tlb-ack-timeout"):
            raise ValueError("M24 fault profile không hợp lệ")
        m23_elf = True
    elif m24_fault != "none":
        raise ValueError("M24 fault profile yêu cầu m24_smp")
    if m23_elf:
        m22_ipc = True
    if m22_ipc:
        m21_syscall = True
    if m21_syscall:
        m20_process = True
    if m20_process:
        m19_scheduler = True
    if m19_scheduler:
        m17_virtio_gpu = True
    if m17_virtio_gpu:
        m16_graphics = True
    if m16_graphics:
        m15_input = True
    if m15_input:
        m14_apic = True
    if m14_apic:
        m13_devices = True
    if m13_devices:
        m12_buddy = True
    if m12_buddy:
        m11_heap = True
    if m11_heap:
        m10_vmm = True
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
    if m11_heap and fault != "none":
        raise ValueError("M11 build profile không nhận fault profile")
    sources=list(SOURCES)
    if m09_pmm:
        sources.extend(("kernel/pmm.c","kernel/pmm.h","tests/m09/build_kernel.py"))
    if m10_vmm:
        sources.extend(("kernel/vmm.c","kernel/vmm.h","tests/m10/build_kernel.py"))
    if m11_heap:
        sources.extend(("kernel/heap.c", "kernel/heap.h",
                        "tests/m11/build_kernel.py", "tests/m11/cpp_probe.cpp"))
    if m12_buddy:
        sources.append("tests/m12/build_kernel.py")
    if m13_devices:
        sources.extend(("kernel/acpi.c", "kernel/acpi.h", "kernel/pci.c",
                        "kernel/pci.h", "kernel/device.c", "kernel/device.h",
                        "tests/m13/build_kernel.py"))
    if m14_apic:
        sources.extend(("kernel/apic.c", "kernel/apic.h", "tests/m14/build_kernel.py"))
    if m15_input:
        sources.extend(("kernel/input.c", "kernel/input.h", "kernel/ps2.c", "kernel/ps2.h", "tests/m15/build_kernel.py"))
    if m16_graphics:
        sources.extend(("kernel/graphics.c", "kernel/graphics.h", "kernel/font8x16.h", "tests/m16/build_kernel.py"))
    if m17_virtio_gpu:
        sources.extend(("kernel/virtio_gpu.c", "kernel/virtio_gpu.h",
                        "tests/m17/build_kernel.py"))
    if m19_scheduler:
        sources.extend(("kernel/scheduler.c", "kernel/scheduler.h",
                        "tests/m19/build_kernel.py"))
    if m20_process:
        sources.extend(("kernel/process.c", "kernel/process.h",
                        "tests/m20/build_kernel.py"))
    if m21_syscall:
        sources.extend(("kernel/syscall.c", "kernel/syscall.h",
                        "kernel/syscall_entry.asm", "user/syscall.c",
                        "user/include/lamix/syscall.h", "tests/m21/build_kernel.py",
                        "tests/m21/user.ld", "tests/m21/user_payload.c",
                        "tests/m21/loop_payload.c",
                        "tests/m21/invalid_return.asm"))
    if m22_ipc:
        sources.extend(("kernel/handle.c", "kernel/handle.h", "kernel/ipc.c",
                        "kernel/ipc.h", "tests/m22/build_kernel.py",
                        "tests/m22/native.c", "tests/m22/CMakeLists.txt",
                        "tests/m22/concurrency.c", "tests/m22/test_m22.py",
                        "tests/m22/user_payload.c", "tests/m22/server_payload.c"))
    if m23_elf:
        sources.extend(("kernel/elf_loader.c", "kernel/elf_loader.h",
                        "kernel/initramfs.c", "kernel/initramfs.h",
                        "tests/m23/build_kernel.py", "tests/m23/CMakeLists.txt",
                        "tests/m23/native.c", "tests/m23/test_m23.py",
                        "tests/m23/alpha.c", "tests/m23/beta.c",
                        "tests/m23/user.ld", "tests/m23/make_initramfs.py",
                        "user/crt0.c", "user/crt0.asm", "user/libc.c",
                        "user/include/stdlib.h", "user/include/string.h"))
    if m24_smp:
        sources.extend(("kernel/ap_trampoline.asm", "tests/m24/build_kernel.py",
                        "tests/m24/native.c", "tests/m24/CMakeLists.txt",
                        "tests/m24/test_m24.py", "tests/m24/concurrency.c",
                        "scripts/lamix_qemu.py",
                        "scripts/qemu_harness.py", "scripts/m17_qemu.py",
                        "scripts/m07_image.py"))
    common=[x["gcc"],"-std=c11","-m64","-mcmodel=kernel","-mno-red-zone","-mgeneral-regs-only","-ffreestanding","-fno-builtin","-fno-pie","-fno-pic","-fno-stack-protector","-fno-asynchronous-unwind-tables","-fno-unwind-tables","-fno-exceptions","-Wall","-Wextra","-Werror","-I",ROOT/"include","-I",ROOT/"kernel","-O2"]
    run("nasm","--reproducible","-f","elf64",*( ["-D", "LAMIX_M14_APIC=1"] if m14_apic else []),"-o",output/"entry.o",ROOT/"kernel/entry.asm")
    run("nasm","--reproducible","-f","elf64",*( ["-D", "LAMIX_M19_SCHEDULER=1"] if m19_scheduler else []), *( ["-D", "LAMIX_M24_SMP=1"] if m24_smp else []),"-o",output/"interrupts.o",ROOT/"kernel/interrupts.asm")
    if m21_syscall:
        run("nasm", "--reproducible", "-f", "elf64", "-o",
            output/"syscall_entry.o", *( ["-D", "LAMIX_M24_SMP=1"] if m24_smp else []),
            ROOT/"kernel/syscall_entry.asm")
        user_flags = [x["gcc"], "-std=c11", "-m64", "-mcmodel=large",
            "-mno-red-zone", "-mgeneral-regs-only", "-ffreestanding",
            "-fno-builtin", "-fno-pie", "-fno-pic", "-fno-stack-protector",
            "-fno-asynchronous-unwind-tables", "-fno-unwind-tables", "-O2",
            "-Wall", "-Wextra", "-Werror", "-I", ROOT/"user/include"]
        if m22_ipc:
            user_flags += ["-DLAMIX_M22_PROFILE=1"]
        if m23_elf:
            m23_flags = [x["gcc"], "-std=c11", "-m64", "-mcmodel=small",
                "-mno-red-zone", "-mgeneral-regs-only", "-ffreestanding",
                "-fno-builtin", "-fpie", "-fno-plt", "-fno-stack-protector",
                "-fno-asynchronous-unwind-tables", "-fno-unwind-tables", "-O2",
                "-Wall", "-Wextra", "-Werror", "-DLAMIX_M22_PROFILE=1",
                "-I", ROOT/"user/include"]
            if m24_smp:
                m23_flags += ["-DLAMIX_M24_SMP=1"]
            for name, source in (("alpha", "tests/m23/alpha.c"),
                                 ("beta", "tests/m23/beta.c")):
                app_objects = []
                for suffix, app_source in (("crt0", "user/crt0.c"),
                                           ("libc", "user/libc.c"),
                                           ("syscall", "user/syscall.c"),
                                           ("app", source)):
                    app_obj = output/f"m23_{name}_{suffix}.o"
                    run(*m23_flags, "-c", ROOT/app_source, "-o", app_obj)
                    app_objects.append(app_obj)
                entry_obj = output/f"m23_{name}_entry.o"
                run("nasm", "--reproducible", "-f", "elf64", "-o",
                    entry_obj, ROOT/"user/crt0.asm")
                app_objects.insert(0, entry_obj)
                app_elf = output/f"{name}.elf"
                run(x["ld"], "-shared", "--no-dynamic-linker", "-Bsymbolic",
                    "-z", "text", "-z", "norelro", "-z", "max-page-size=0x1000",
                    "--build-id=none", "-T", ROOT/"tests/m23/user.ld",
                    "-o", app_elf, *app_objects)
                app_inspection = subprocess.check_output(
                    [str(x["readelf"]), "-h", "-l", "-d", "-r", str(app_elf)],
                    text=True)
                (output/f"{name}.inspection.txt").write_text(app_inspection)
                if not any(line.strip().startswith("Type:") and
                           line.split()[1] == "DYN"
                           for line in app_inspection.splitlines()):
                    raise ValueError(f"M23 {name} không phải ELF ET_DYN")
                if "INTERP" in app_inspection or "NEEDED" in app_inspection or "JUMP_SLOT" in app_inspection:
                    raise ValueError(f"M23 {name} có dynamic linking không được hỗ trợ")
                for line in app_inspection.splitlines():
                    if "R_X86_64_" in line and "R_X86_64_RELATIVE" not in line:
                        raise ValueError(f"M23 {name} relocation không hỗ trợ: {line.strip()}")
            archive = output/"initramfs.cpio"
            run(sys.executable, ROOT/"tests/m23/make_initramfs.py", "--output", archive,
                "--entry", "alpha.elf", output/"alpha.elf",
                "--entry", "beta.elf", output/"beta.elf")
            init_asm = output/"initramfs.asm"
            init_asm.write_text("BITS 64\nsection .rodata\nalign 16\nglobal m23_initramfs_start\nglobal m23_initramfs_end\nm23_initramfs_start:\nincbin \"" + str(archive) + "\"\nm23_initramfs_end:\n")
            initramfs_blob = output/"initramfs_blob.o"
            run("nasm", "--reproducible", "-f", "elf64", "-o",
                initramfs_blob, init_asm)
        if m24_smp:
            trampoline_bin = output/"ap_trampoline.bin"
            run("nasm", "--reproducible", "-f", "bin", "-o",
                trampoline_bin, ROOT/"kernel/ap_trampoline.asm")
            blob_asm = output/"ap_trampoline_blob.asm"
            blob_asm.write_text("BITS 64\nsection .rodata\nalign 16\nglobal m24_ap_trampoline_start\nglobal m24_ap_trampoline_end\nm24_ap_trampoline_start:\nincbin \"" + str(trampoline_bin) + "\"\nm24_ap_trampoline_end:\n")
            trampoline_obj = output/"ap_trampoline_blob.o"
            run("nasm", "--reproducible", "-f", "elf64", "-o",
                trampoline_obj, blob_asm)
        else:
            wrapper_obj = output/"user_syscall.o"
            run(*user_flags, "-c", ROOT/"user/syscall.c", "-o", wrapper_obj)
            user_apps = []
            payload_sources = (("test", "tests/m22/user_payload.c" if m22_ipc else "tests/m21/user_payload.c"),
                               ("loop", "tests/m22/server_payload.c" if m22_ipc else "tests/m21/loop_payload.c"))
            for name, source in payload_sources:
                app_obj = output/f"user_{name}.o"
                app_elf = output/f"user_{name}.elf"
                app_bin = output/f"user_{name}.bin"
                run(*user_flags, "-c", ROOT/source, "-o", app_obj)
                run(x["ld"], "--fatal-warnings", "--build-id=none", "-z",
                    "noexecstack", "-T", ROOT/"tests/m21/user.ld", "-o", app_elf,
                    wrapper_obj, app_obj)
                run(x["objcopy"], "-O", "binary", app_elf, app_bin)
                if app_bin.stat().st_size > 4096:
                    raise ValueError(f"payload user {name} vượt một trang code")
                user_apps.append((name, app_bin.read_bytes()))
            bad_return = output/"user_invalid_return.bin"
            run("nasm", "--reproducible", "-f", "bin", "-o", bad_return,
                ROOT/"tests/m21/invalid_return.asm")
            user_apps.append(("invalid_return", bad_return.read_bytes()))
            header = ["/* Generated in build/: embedded M21 ring 3 payloads. */",
                      "#ifndef LAMIX_M21_PAYLOADS_H", "#define LAMIX_M21_PAYLOADS_H",
                      "#include <stddef.h>", "#include <stdint.h>"]
            for name, data in user_apps:
                values = ",".join(f"0x{byte:02x}" for byte in data)
                header.extend((f"static const uint8_t m21_{name}_payload[] = {{{values}}};",
                               f"static const size_t m21_{name}_payload_size = sizeof(m21_{name}_payload);"))
            header.append("#endif")
            (output/"m21_payloads.h").write_text("\n".join(header) + "\n")
    objects=[]
    compile_sources=["kernel/kernel.c","lib/boot/contract.c","lib/boot/memory.c"]
    if m09_pmm:
        compile_sources.append("kernel/pmm.c")
    if m10_vmm:
        compile_sources.append("kernel/vmm.c")
    if m11_heap:
        compile_sources.append("kernel/heap.c")
    if m13_devices:
        compile_sources.extend(("kernel/acpi.c", "kernel/pci.c",
                                "kernel/device.c"))
    if m14_apic:
        compile_sources.append("kernel/apic.c")
    if m15_input:
        compile_sources.extend(("kernel/input.c", "kernel/ps2.c"))
    if m16_graphics:
        compile_sources.append("kernel/graphics.c")
    if m17_virtio_gpu:
        compile_sources.append("kernel/virtio_gpu.c")
    if m19_scheduler:
        compile_sources.append("kernel/scheduler.c")
    if m20_process:
        compile_sources.append("kernel/process.c")
    if m22_ipc:
        sources.append("kernel/handle.c")
        compile_sources.append("kernel/handle.c")
    if m21_syscall:
        compile_sources.append("kernel/syscall.c")
    if m22_ipc:
        compile_sources.append("kernel/ipc.c")
    if m23_elf:
        compile_sources.extend(("kernel/elf_loader.c", "kernel/initramfs.c"))
    for source in compile_sources:
        obj=output/(Path(source).stem+".o"); args=common.copy()
        if source=="kernel/kernel.c" and not m10_vmm and variants[fault]: args += ["-D"+variants[fault]+"=1"]
        if source=="kernel/kernel.c" and m09_pmm: args += ["-DLAMIX_M09_PMM=1"]
        if source=="kernel/kernel.c" and m10_vmm:
            args += ["-DLAMIX_M10_VMM=1"]
            if m10_variants[fault]: args += ["-D"+m10_variants[fault]+"=1"]
        if source=="kernel/kernel.c" and m11_heap:
            args += ["-DLAMIX_M11_HEAP=1"]
        if source=="kernel/kernel.c" and m12_buddy:
            args += ["-DLAMIX_M12_BUDDY=1"]
        if source=="kernel/kernel.c" and m13_devices:
            args += ["-DLAMIX_M13_DEVICES=1"]
        if source=="kernel/kernel.c" and m14_apic:
            args += ["-DLAMIX_M14_APIC=1"]
        if source=="kernel/kernel.c" and m15_input:
            args += ["-DLAMIX_M15_INPUT=1"]
        if source=="kernel/kernel.c" and m16_graphics:
            args += ["-DLAMIX_M16_GRAPHICS=1"]
        if source=="kernel/kernel.c" and m17_virtio_gpu:
            args += ["-DLAMIX_M17_VIRTIO_GPU=1"]
        if source=="kernel/kernel.c" and m19_scheduler:
            args += ["-DLAMIX_M19_SCHEDULER=1"]
        if source=="kernel/kernel.c" and m20_process:
            args += ["-DLAMIX_M20_PROCESS=1"]
        if source=="kernel/kernel.c" and m21_syscall:
            args += ["-DLAMIX_M21_SYSCALL=1", "-I", output]
        if source=="kernel/kernel.c" and m22_ipc:
            args += ["-DLAMIX_M22_IPC=1"]
        if source=="kernel/kernel.c" and m23_elf:
            args += ["-DLAMIX_M23_ELF=1"]
        if source=="kernel/kernel.c" and m24_smp:
            args += ["-DLAMIX_M24_SMP=1", f"-DLAMIX_M24_MAX_CPUS={max_cpus}"]
            if m24_fault == "ap-timeout":
                args += ["-DLAMIX_M24_FAULT_AP_TIMEOUT=1"]
            elif m24_fault == "tlb-ack-timeout":
                args += ["-DLAMIX_M24_FAULT_TLB_ACK_TIMEOUT=1"]
        if source=="kernel/process.c" and m22_ipc:
            args += ["-DLAMIX_M22_IPC=1"]
        if source=="kernel/process.c" and m23_elf:
            args += ["-DLAMIX_M23_ELF=1"]
        if source=="kernel/syscall.c" and m22_ipc:
            args += ["-DLAMIX_M22_IPC=1"]
        if source=="kernel/pci.c" and m13_devices:
            args += ["-DLAMIX_KERNEL=1"]
        run(*args,"-c",ROOT/source,"-o",obj);objects.append(obj)
    if m11_heap:
        cpp_object = output / "cpp_probe.o"
        cpp_args = common.copy()
        cpp_args.pop(0)
        cpp_args[0] = "-std=c++17"
        cpp_args += ["-fno-rtti", "-fno-threadsafe-statics", "-fno-use-cxa-atexit"]
        run(x["g++"], *cpp_args, "-c", ROOT/"tests/m11/cpp_probe.cpp",
            "-o", cpp_object)
        objects.append(cpp_object)
    elf=output/"kernel.elf"
    run(x["ld"],"--fatal-warnings","--build-id=none","-z","noexecstack","-z","max-page-size=0x1000","-T",ROOT/"kernel/linker.ld","-Map",output/(elf.stem+".map"),"-o",elf,output/"entry.o",output/"interrupts.o",*( [output/"syscall_entry.o"] if m21_syscall else []),*( [output/"initramfs_blob.o"] if m23_elf else []),*( [output/"ap_trampoline_blob.o"] if m24_smp else []),*objects)
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
    if m11_heap:
        cpp_undefined=subprocess.check_output([str(x["nm"]),"-u",str(output/"cpp_probe.o")],text=True).strip()
        cpp_symbols={line.split()[-1] for line in cpp_undefined.splitlines() if line.split()}
        if cpp_symbols - {"kheap_alloc", "kheap_free"}:
            raise ValueError("C++ consumer cần runtime symbols ngoài C heap API: "+cpp_undefined)
        sections=subprocess.check_output([str(x["readelf"]),"-S",str(elf)],text=True)
        if ".init_array" in sections or ".ctors" in sections:
            raise ValueError("kernel M11 không được có static constructors")
    if b"INTERP" in subprocess.check_output([str(x["readelf"]),"-l",str(elf)]): raise ValueError("PT_INTERP không freestanding")
    report={"status":"PASS","mode":"m24-kernel" if m24_smp else "m23-kernel" if m23_elf else "m22-kernel" if m22_ipc else "m21-kernel" if m21_syscall else "m20-kernel" if m20_process else "m19-kernel" if m19_scheduler else "m17-kernel" if m17_virtio_gpu else "m16-kernel" if m16_graphics else "m15-kernel" if m15_input else "m14-kernel" if m14_apic else "m13-kernel" if m13_devices else "m12-kernel" if m12_buddy else "m11-kernel" if m11_heap else "m10-kernel" if m10_vmm else "m09-kernel" if m09_pmm else "m08-kernel","fault":fault,"artifact":elf.name,"commands":commands,
            "tool_versions":{"gcc":subprocess.check_output([str(x["gcc"]),"--version"],text=True).splitlines()[0],"g++":subprocess.check_output([str(x["g++"]),"--version"],text=True).splitlines()[0] if m11_heap else None,"ld":subprocess.check_output([str(x["ld"]),"--version"],text=True).splitlines()[0],"nasm":subprocess.check_output(["nasm","-v"],text=True).strip()},
            "source_hashes":{s:sha(ROOT/s) for s in sources},"hashes":{elf.name:sha(elf), **({"initramfs.cpio":sha(output/"initramfs.cpio"),"alpha.elf":sha(output/"alpha.elf"),"beta.elf":sha(output/"beta.elf")} if m23_elf else {})},"elf_inspection":elf.stem+".inspection.txt","load_segments":loads}
    if m24_smp:
        report["max_cpus"] = max_cpus
        report["m24_fault"] = m24_fault
    (output/"build.json").write_text(json.dumps(report,ensure_ascii=False,indent=2)+"\n")
    print(json.dumps(report,ensure_ascii=False,indent=2))
def main():
    p=argparse.ArgumentParser();p.add_argument("--build",type=Path,required=True);p.add_argument("--fault",choices=("none","ud","pf"),default="none");a=p.parse_args();build(a.build,a.fault)
if __name__=="__main__":main()
