# Lamix OS

[![Documentation CI](https://github.com/duclam2011dz/LamixOS/actions/workflows/docs-ci.yml/badge.svg?branch=main)](https://github.com/duclam2011dz/LamixOS/actions/workflows/docs-ci.yml)

Hệ điều hành Gaming native x86_64 phát triển dài hạn, dùng NASM, C và freestanding C++. Định hướng: boot BIOS/UEFI tự viết, Hybrid Kernel và hệ sinh thái game native.

## Trạng thái

M00 đã đạt nghiệm thu tài liệu, GitHub Public, GPL và Documentation CI. Milestone tag M00 chờ xác nhận hoàn thành riêng của người dùng. Skill `docs-consistency` đã cài và được kiểm tra. Chưa có bootloader, kernel, game, hệ thống build hoặc test harness OS. Chưa có bản phát hành và chưa đạt Playable.

## Bắt đầu

Đọc [mục tiêu](GOALS.md), [roadmap](ROADMAP.md), [kế hoạch M00](PLANS.md) và [handoff](docs/SESSION_HANDOFF.md). Người đóng góp xem [CONTRIBUTING.md](CONTRIBUTING.md); Codex đọc [AGENTS.md](AGENTS.md).

Host mục tiêu là Windows + WSL2, ưu tiên workspace trên filesystem Linux. Toolchain dự kiến dùng GCC cross-compiler, Binutils, NASM, CMake/Ninja/Python; kiểm chứng và thiết lập thuộc M01. Hiện chưa có lệnh build/run OS. Các kiểm tra tài liệu có trong [TESTING](docs/TESTING.md).

## Repository và giấy phép

Repository chính thức: [duclam2011dz/LamixOS](https://github.com/duclam2011dz/LamixOS), public, nhánh `main`. Kết quả push và CI thực tế được ghi ở [handoff](docs/SESSION_HANDOFF.md).

Dự án dùng **GPL-3.0-only**, GNU General Public License version 3, không cấp lựa chọn phiên bản mới hơn. Văn bản tiếng Anh chính thức nằm ở [LICENSE](LICENSE). Chưa có thành phần OS bên thứ ba được nhập vào dự án; toolchain/firmware trên host không được phân phối trong repository.

Kiểm tra tài liệu tại local:

```bash
python3 scripts/validate_docs.py
python3 -m unittest discover -s tests -p 'test_validate_docs.py'
```
