# Lamix OS

[![Harness CI](https://github.com/duclam2011dz/LamixOS/actions/workflows/harness-ci.yml/badge.svg?branch=main)](https://github.com/duclam2011dz/LamixOS/actions/workflows/harness-ci.yml)

[![Toolchain CI](https://github.com/duclam2011dz/LamixOS/actions/workflows/toolchain-ci.yml/badge.svg?branch=main)](https://github.com/duclam2011dz/LamixOS/actions/workflows/toolchain-ci.yml)

[![Documentation CI](https://github.com/duclam2011dz/LamixOS/actions/workflows/docs-ci.yml/badge.svg?branch=main)](https://github.com/duclam2011dz/LamixOS/actions/workflows/docs-ci.yml)

Hệ điều hành Gaming native x86_64 phát triển dài hạn, dùng NASM, C và freestanding C++. Định hướng: boot BIOS/UEFI tự viết, Hybrid Kernel và hệ sinh thái game native.

## Trạng thái

M00–M17, M19–M24 đã nghiệm thu; M18 vẫn PLANNED. M22 giữ profile handles/capabilities, channel IPC và SHM riêng; M23 bổ sung Static PIE ELF64 loader/runtime; M24 bổ sung SMP. `latest` hiện trỏ profile M24, các profile cũ vẫn chọn tường minh. Xem [handoff](docs/SESSION_HANDOFF.md) để biết provenance và giới hạn. Đây chưa phải kernel sản phẩm hoàn chỉnh hoặc OS hoàn chỉnh; chưa có bản phát hành và chưa đạt Playable.

## Bắt đầu

Đọc [mục tiêu](GOALS.md), [roadmap](ROADMAP.md), [kế hoạch hiện hành](PLANS.md) và [handoff](docs/SESSION_HANDOFF.md). Người đóng góp xem [CONTRIBUTING.md](CONTRIBUTING.md); Codex đọc [AGENTS.md](AGENTS.md).

Host mục tiêu là Windows + WSL2, ưu tiên workspace trên filesystem Linux. BIOS/kernel ELF dùng GCC cross-compiler và Binutils; EFI PE/COFF build dùng Clang+LLD; NASM, CMake/Ninja/Python dùng cho cả suites. Hướng dẫn local và CI ở [TOOLCHAIN](docs/TOOLCHAIN.md). Chưa có lệnh run/boot OS sản phẩm; M05/M06 guest probes là kiểm thử bootloader. Các kiểm tra tài liệu có trong [TESTING](docs/TESTING.md).

## Repository và giấy phép

Repository chính thức: [duclam2011dz/LamixOS](https://github.com/duclam2011dz/LamixOS), public, nhánh `main`. Kết quả push và CI thực tế được ghi ở [handoff](docs/SESSION_HANDOFF.md).

Dự án dùng **GPL-3.0-only**, GNU General Public License version 3, không cấp lựa chọn phiên bản mới hơn. Văn bản tiếng Anh chính thức nằm ở [LICENSE](LICENSE). Chưa có thành phần OS bên thứ ba được nhập vào dự án; toolchain/firmware trên host không được phân phối trong repository.

Kiểm tra tài liệu tại local:

```bash
python3 scripts/validate_docs.py
python3 -m unittest discover -s tests -p 'test_validate_docs.py'
```

## Build sample M01

Sau khi chuẩn bị dependencies theo TOOLCHAIN, chạy tại root:

```bash
. .venv/bin/activate
python scripts/doctor.py --profile wsl2 --json out/m01/doctor.json
python scripts/smoke.py --reproducible
```

Sample chỉ được compile/link và kiểm tra ELF; không chạy trực tiếp hoặc boot. Logs/hashes nằm trong out/m01, không được commit. Kết quả CI thực tế và trạng thái M01 tại [handoff](docs/SESSION_HANDOFF.md).

## QEMU harness M02

M02 đã đạt kiểm chứng harness local/CI, gồm 33 fixture tests và hai QEMU launch-only profiles. Annotated tag [milestone/M02](https://github.com/duclam2011dz/LamixOS/tree/milestone/M02) đã được người dùng duyệt và push; không phải bằng chứng OS đã boot. Sau khi kích hoạt .venv:

```bash
python scripts/run_qemu.py --profile bios --launch-only
python scripts/run_qemu.py --profile uefi --launch-only
cmake -S tests/qemu -B build/m02 -DPython3_EXECUTABLE="$PWD/.venv/bin/python"
ctest --test-dir build/m02 --no-tests=error --output-on-failure
```

Launch-only dùng TCG và -S, xác minh QMP rồi đóng tiến trình. Boot skeleton chưa có image sẽ SKIPPED, exit 77. Logs và result.json mỗi run ở out/m02; [TESTING](docs/TESTING.md) mô tả kết quả/timeout/cleanup và CI thực tế tại [handoff](docs/SESSION_HANDOFF.md).

## Contract M03

LBP v1 có headers C11/C++17, thư viện validation/normalization freestanding và host GPT model; M05 bổ sung BIOS Stage2/kernel probe test-only, chưa có kernel sản phẩm/image generator. Xem [BOOT_PROTOCOL](docs/BOOT_PROTOCOL.md), [IMAGE_LAYOUT](docs/IMAGE_LAYOUT.md) và [lệnh kiểm thử](docs/TESTING.md#boot-contract-m03). Host tests và cross compile không chứng minh OS boot. Trạng thái nghiệm thu thực tế tại handoff.

## BIOS Stage1 M04

Stage1 NASM và fixture tests: [đặc tả/audit](docs/BIOS_STAGE1.md), [lệnh kiểm thử](docs/TESTING.md#bios-stage1-m04). Fixture sparse 8 GiB là test-only, không phải image OS. Trạng thái local/CI thực tế ở handoff; milestone/M04 đã push; M05 đã DONE theo xác nhận của người dùng.

## BIOS Stage2 M05

BIOS loader nạp ELF64 từ ESP FAT32 và chuyển vào kernel qua LBP v1; [audit/giới hạn](docs/BIOS_STAGE2.md), [kiểm thử](docs/TESTING.md#bios-stage2-m05). M05 DONE; xem [PR #3](https://github.com/duclam2011dz/LamixOS/pull/3) và annotated [milestone/M05](https://github.com/duclam2011dz/LamixOS/tree/milestone/M05). M06–M12 DONE.
