# Lamix OS

[![Toolchain CI](https://github.com/duclam2011dz/LamixOS/actions/workflows/toolchain-ci.yml/badge.svg?branch=main)](https://github.com/duclam2011dz/LamixOS/actions/workflows/toolchain-ci.yml)

[![Documentation CI](https://github.com/duclam2011dz/LamixOS/actions/workflows/docs-ci.yml/badge.svg?branch=main)](https://github.com/duclam2011dz/LamixOS/actions/workflows/docs-ci.yml)

Hệ điều hành Gaming native x86_64 phát triển dài hạn, dùng NASM, C và freestanding C++. Định hướng: boot BIOS/UEFI tự viết, Hybrid Kernel và hệ sinh thái game native.

## Trạng thái

M00 đã đạt nghiệm thu tài liệu, GitHub Public, GPL và Documentation CI. Người dùng đã xác nhận hoàn thành; annotated tag [milestone/M00](https://github.com/duclam2011dz/LamixOS/tree/milestone/M00) ghi nhận commit nghiệm thu. Skill `docs-consistency` đã cài và được kiểm tra. M01 đã đạt kiểm chứng local và Toolchain CI cho build sample freestanding; tag M01 chờ xác nhận hoàn thành riêng; chưa có bootloader, kernel, game hoặc test harness boot OS. Chưa có bản phát hành và chưa đạt Playable.

## Bắt đầu

Đọc [mục tiêu](GOALS.md), [roadmap](ROADMAP.md), [kế hoạch M00](PLANS.md) và [handoff](docs/SESSION_HANDOFF.md). Người đóng góp xem [CONTRIBUTING.md](CONTRIBUTING.md); Codex đọc [AGENTS.md](AGENTS.md).

Host mục tiêu là Windows + WSL2, ưu tiên workspace trên filesystem Linux. Toolchain dùng GCC cross-compiler, Binutils, NASM, CMake/Ninja/Python; hướng dẫn local và CI ở [TOOLCHAIN](docs/TOOLCHAIN.md). Chưa có lệnh run/boot OS. Các kiểm tra tài liệu có trong [TESTING](docs/TESTING.md).

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
