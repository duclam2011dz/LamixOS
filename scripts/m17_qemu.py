#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Build image M17 rồi chạy QEMU SeaBIOS, OVMF hoặc cả hai."""
import argparse
from datetime import datetime
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from scripts.m07_image import verify_build_report


def project_path(value):
    path = Path(value)
    return path if path.is_absolute() else ROOT / path


def run_step(command, capture=False):
    command = [str(part) for part in command]
    print("$ " + " ".join(command), flush=True)
    if not capture:
        subprocess.run(command, cwd=ROOT, check=True)
        return ""
    result = subprocess.run(command, cwd=ROOT, text=True,
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if result.returncode:
        print(result.stdout, file=sys.stderr, flush=True)
        raise subprocess.CalledProcessError(result.returncode, command)
    return result.stdout


def build_if_needed(name, build_dir, artifacts, command, valid=None):
    try:
        report = verify_build_report(build_dir, artifacts)
        if valid is not None and not valid(report):
            raise ValueError(f"build profile không khớp: {build_dir}")
        print(f"PASS: dùng lại build đã xác minh ({name})")
        return report
    except (OSError, ValueError, KeyError, json.JSONDecodeError):
        run_step(command, capture=name == "kernel")
        report = verify_build_report(build_dir, artifacts)
        if valid is not None and not valid(report):
            raise ValueError(f"build profile không hợp lệ sau build: {build_dir}")
        return report


def build_image(build_root):
    build_root = build_root.resolve()
    if not build_root.is_relative_to(ROOT / "build"):
        raise ValueError("--build-dir phải nằm trong build/")
    kernel_build = build_root / "kernel"
    bios_build = build_root / "bios"
    uefi_build = build_root / "uefi"
    report = build_if_needed("kernel", kernel_build, ("kernel.elf",),
        [sys.executable, "tests/m17/build_kernel.py", "--build", kernel_build],
        lambda item: item.get("mode") == "m17-kernel")
    bios_report = build_if_needed("BIOS Stage2 M16", bios_build,
        ("stage1.bin", "stage2.bin", "kernel.elf"),
        [sys.executable, "tests/bios_stage2/build_stage2.py", "--build",
         bios_build, "--profile", "Release", "--m16-video"],
        lambda item: item.get("m16_video") is True and item.get("profile") == "Release")
    uefi_report = build_if_needed("UEFI loader", uefi_build, ("BOOTX64.EFI",),
        [sys.executable, "tests/uefi/build_uefi.py", "--build", uefi_build])
    kernel_hash = report["hashes"]["kernel.elf"]
    stage2_hash = bios_report["hashes"]["stage2.bin"]
    efi_hash = uefi_report["hashes"]["BOOTX64.EFI"]
    output = ROOT / "out/m17"
    output.mkdir(parents=True, exist_ok=True)
    image = output / f"lamix-m17-{kernel_hash[:12]}-{stage2_hash[:8]}-{efi_hash[:8]}.img"
    if image.exists():
        action = "check"
        print(f"Image đã có; kiểm tra và giữ nguyên: {image}")
    else:
        action = "create"
        print(f"Tạo image raw GPT dùng chung BIOS/UEFI: {image}")
    run_step([sys.executable, "scripts/m07_image.py", action, "--image", image,
              "--bios-build", bios_build, "--uefi-build", uefi_build,
              "--kernel-build", kernel_build,
              *( ["--seed", "lamix-m17-direct-qemu"] if action == "create" else [] )],
             capture=True)
    print(f"PASS: image {action} {image}")
    return image


def qemu_command(firmware, image, run_dir, qemu, seabios, ovmf_code,
                 ovmf_vars, cpus=1):
    if type(cpus) is not int or not 1 <= cpus <= 16:
        raise ValueError("cpus phải nằm trong khoảng 1..16")
    command = [qemu, "-machine", "pc" if firmware == "bios" else "q35",
               "-accel", "tcg", "-smp", str(cpus), "-m", "256M",
               "-nic", "none", "-monitor", "none", "-no-reboot",
               "-no-shutdown", "-vga", "none", "-device", "virtio-vga"]
    if firmware == "bios":
        command += ["-bios", str(seabios)]
    else:
        private_vars = run_dir / "OVMF_VARS.fd"
        shutil.copyfile(ovmf_vars, private_vars)
        command += ["-drive", f"if=pflash,format=raw,unit=0,readonly=on,file={ovmf_code}",
                    "-drive", f"if=pflash,format=raw,unit=1,file={private_vars}"]
    command += ["-drive", f"file={image},format=raw,if=ide,snapshot=on",
                "-serial", "stdio"]
    return command


def run_guest(firmware, image, args):
    run_root = ROOT / "out/m17/runs"
    run_root.mkdir(parents=True, exist_ok=True)
    stamp = datetime.now().strftime("%Y%m%d-%H%M%S")
    run_dir = Path(tempfile.mkdtemp(prefix=f"manual-{firmware}-{stamp}-",
                                    dir=run_root))
    command = qemu_command(firmware, image, run_dir, args.qemu,
                           args.seabios, args.ovmf_code, args.ovmf_vars)
    env = os.environ.copy()
    env["TMPDIR"] = str(run_dir)
    print(f"\nKhởi chạy {firmware.upper()} từ {image}")
    print("Serial sẽ hiện trong terminal; chờ marker M17:PASS.")
    print("$ " + " ".join(command), flush=True)
    print(f"Run data: {run_dir}", flush=True)
    result = subprocess.run(command, cwd=ROOT, env=env)
    if result.returncode:
        print(f"QEMU {firmware.upper()} kết thúc với mã {result.returncode}",
              file=sys.stderr)
    return result.returncode


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--firmware", choices=("bios", "uefi", "both"),
                        default="both", help="mặc định chạy lần lượt BIOS rồi UEFI")
    parser.add_argument("--build-dir", type=Path, default=Path("build/m17-qemu"))
    parser.add_argument("--build-only", action="store_true",
                        help="build kernel/loaders/image nhưng không mở QEMU")
    parser.add_argument("--qemu", default=os.environ.get("LAMIX_QEMU", "qemu-system-x86_64"))
    parser.add_argument("--seabios", type=Path,
                        default=Path(os.environ.get("LAMIX_SEABIOS", "/usr/share/seabios/bios.bin")))
    parser.add_argument("--ovmf-code", type=Path,
                        default=Path(os.environ.get("LAMIX_OVMF_CODE", "/usr/share/OVMF/OVMF_CODE.fd")))
    parser.add_argument("--ovmf-vars", type=Path,
                        default=Path(os.environ.get("LAMIX_OVMF_VARS", "/usr/share/OVMF/OVMF_VARS.fd")))
    args = parser.parse_args()
    build_root = project_path(args.build_dir)
    args.seabios = project_path(args.seabios).resolve()
    args.ovmf_code = project_path(args.ovmf_code).resolve()
    args.ovmf_vars = project_path(args.ovmf_vars).resolve()
    image = build_image(build_root)
    print(f"\nImage M17 sẵn sàng: {image}")
    if args.build_only:
        return 0
    if args.firmware in ("bios", "both") and not args.seabios.is_file():
        raise FileNotFoundError(f"không tìm thấy SeaBIOS: {args.seabios}; dùng --seabios")
    if args.firmware in ("uefi", "both"):
        for firmware_file in (args.ovmf_code, args.ovmf_vars):
            if not firmware_file.is_file():
                raise FileNotFoundError(f"không tìm thấy OVMF file: {firmware_file}")
    selected = ("bios", "uefi") if args.firmware == "both" else (args.firmware,)
    for firmware in selected:
        status = run_guest(firmware, image, args)
        if status:
            return status
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        raise SystemExit(1)
