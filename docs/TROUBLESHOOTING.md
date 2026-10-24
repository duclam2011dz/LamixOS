# Chẩn đoán môi trường

## Quan sát trong M00 — 2026-10-24

Host là WSL2 x86_64. `.git` và `.agents` rỗng, chỉ đọc; `git status` báo `not a git repository`. Root workspace cho phép tạo tài liệu. Chưa thử thay đổi quyền hoặc lách sandbox.

Chẩn đoán đọc:

```bash
ls -ld .git .agents
git rev-parse --show-toplevel
command -v cmake ninja ctest pytest
python3 -m pytest --version
```

Hướng xử lý Git/skill: người dùng hoặc môi trường cấp quyền ghi đúng đường dẫn; sau đó kiểm tra lại nội dung trước khi khởi tạo/cài. Đã thử lại cùng ngày: thao tác trong sandbox vẫn báo Read-only file system; git init và tạo/cài skill thành công qua cơ chế quyền nâng cao được môi trường chấp thuận. Repository hiện ở branch main và skill đã cài; đây không còn là blocker M00. Không suy ra sandbox đã cho ghi trực tiếp vào hai thư mục.

CMake/Ninja/CTest/pytest chưa tìm thấy trong PATH ở lượt khảo sát; Python báo không có module pytest. Cross-compilers, NASM, QEMU, GDB, lint tools và firmware có đường dẫn nhưng chưa kiểm chứng phối hợp. Xác minh/cài toolchain thuộc M01; không dùng GCC host thay cross-compiler để bỏ qua lỗi.

## Lỗi OS

**Chưa triển khai.** Chưa có panic, serial logs hoặc QEMU profile OS để chẩn đoán. Khi có lỗi thật, ghi lệnh tái hiện và bằng chứng; không thêm cách sửa suy đoán như kết quả đã kiểm chứng.
