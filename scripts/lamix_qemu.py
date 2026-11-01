#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Build image rồi chạy Lamix OS mới nhất (mặc định M20) trên QEMU."""
import argparse
from pathlib import Path
import os
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from scripts.m17_qemu import (build_if_needed, build_image as build_m17_image,
                              project_path, qemu_command, run_step)

LATEST_PROFILE = "m20"


def resolve_profile(profile):
    return LATEST_PROFILE if profile == "latest" else profile


def build_m19_image(build_root):
    build_root = build_root.resolve()
    if not build_root.is_relative_to(ROOT / "build"):
        raise ValueError("--build-dir phải nằm trong build/")
    kernel_build = build_root / "kernel"
    bios_build = build_root / "bios"
    uefi_build = build_root / "uefi"
    report = build_if_needed("kernel M19", kernel_build, ("kernel.elf",),
        [sys.executable, "tests/m19/build_kernel.py", "--build", kernel_build],
        lambda item: item.get("mode") == "m19-kernel")
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
    output = ROOT / "out/m19"
    output.mkdir(parents=True, exist_ok=True)
    image = output / f"lamix-m19-{kernel_hash[:12]}-{stage2_hash[:8]}-{efi_hash[:8]}.img"
    if image.exists():
        action = "check"
        print(f"Image đã có; kiểm tra và giữ nguyên: {image}")
    else:
        action = "create"
        print(f"Tạo image raw GPT dùng chung BIOS/UEFI: {image}")
    run_step([sys.executable, "scripts/m07_image.py", action, "--image", image,
              "--bios-build", bios_build, "--uefi-build", uefi_build,
              "--kernel-build", kernel_build,
              *( ["--seed", "lamix-m19-qemu"] if action == "create" else [] )],
             capture=True)
    print(f"PASS: image {action} {image}")
    return image


def build_m20_image(build_root):
    build_root = build_root.resolve()
    if not build_root.is_relative_to(ROOT / "build"):
        raise ValueError("--build-dir phải nằm trong build/")
    kernel_build = build_root / "kernel"
    bios_build = build_root / "bios"
    uefi_build = build_root / "uefi"
    report = build_if_needed("kernel M20", kernel_build, ("kernel.elf",),
        [sys.executable, "tests/m20/build_kernel.py", "--build", kernel_build],
        lambda item: item.get("mode") == "m20-kernel")
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
    output = ROOT / "out/m20"
    output.mkdir(parents=True, exist_ok=True)
    image = output / f"lamix-m20-{kernel_hash[:12]}-{stage2_hash[:8]}-{efi_hash[:8]}.img"
    action = "check" if image.exists() else "create"
    run_step([sys.executable, "scripts/m07_image.py", action, "--image", image,
              "--bios-build", bios_build, "--uefi-build", uefi_build,
              "--kernel-build", kernel_build,
              *( ["--seed", "lamix-m20-qemu"] if action == "create" else [] )],
             capture=True)
    print(f"PASS: image {action} {image}")
    return image


def build_image(profile, build_root):
    if profile == "m17":
        return build_m17_image(build_root)
    if profile == "m19":
        return build_m19_image(build_root)
    if profile == "m20":
        return build_m20_image(build_root)
    raise ValueError(f"profile không được hỗ trợ: {profile}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--profile", choices=("latest", "m17", "m19", "m20"),
                        default="latest", help="latest hiện trỏ tới M20")
    parser.add_argument("--firmware", choices=("bios", "uefi", "both"),
                        default="both", help="mặc định chạy BIOS rồi UEFI")
    parser.add_argument("--build-dir", type=Path,
                        help="mặc định build/lamix-qemu-<profile>")
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
    profile = resolve_profile(args.profile)
    build_dir = args.build_dir or Path(f"build/lamix-qemu-{profile}")
    image = build_image(profile, project_path(build_dir))
    print(f"\nImage {profile.upper()} sẵn sàng: {image}")
    if args.build_only:
        return 0
    args.seabios = project_path(args.seabios).resolve()
    args.ovmf_code = project_path(args.ovmf_code).resolve()
    args.ovmf_vars = project_path(args.ovmf_vars).resolve()
    if args.firmware in ("bios", "both") and not args.seabios.is_file():
        raise FileNotFoundError(f"không tìm thấy SeaBIOS: {args.seabios}; dùng --seabios")
    if args.firmware in ("uefi", "both"):
        for firmware_file in (args.ovmf_code, args.ovmf_vars):
            if not firmware_file.is_file():
                raise FileNotFoundError(f"không tìm thấy OVMF file: {firmware_file}")
    run_root = ROOT / f"out/{profile}/runs"
    run_root.mkdir(parents=True, exist_ok=True)
    selected = ("bios", "uefi") if args.firmware == "both" else (args.firmware,)
    for firmware in selected:
        import tempfile
        run_dir = Path(tempfile.mkdtemp(prefix=f"manual-{firmware}-",
                                        dir=run_root))
        command = qemu_command(firmware, image, run_dir, args.qemu,
                               args.seabios, args.ovmf_code, args.ovmf_vars)
        env = os.environ.copy()
        env["TMPDIR"] = str(run_dir)
        marker = {"m17": "M17:PASS", "m19": "M19:PASS",
                  "m20": "M20:PASS"}[profile]
        print(f"\nKhởi chạy {firmware.upper()} ({profile.upper()}) từ {image}")
        print(f"Serial sẽ hiện trong terminal; chờ marker {marker}.")
        print("$ " + " ".join(command), flush=True)
        print(f"Run data: {run_dir}", flush=True)
        result = subprocess.run(command, cwd=ROOT, env=env)
        if result.returncode:
            print(f"QEMU {firmware.upper()} kết thúc với mã {result.returncode}",
                  file=sys.stderr)
            return result.returncode
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        raise SystemExit(1)
