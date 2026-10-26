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

## Toolchain M01

Nếu sudo yêu cầu mật khẩu, không gửi mật khẩu trong chat. Dùng dependencies cô lập đã ghim theo [TOOLCHAIN](TOOLCHAIN.md); không sửa Python hệ thống hoặc thay cross-toolchain. Kích hoạt .venv trước khi gọi doctor/CMake để tìm đúng công cụ.

Nếu CMake cache trỏ compiler cũ, dùng build directory mới đã xác nhận thay vì sửa lịch sử hoặc xóa tùy tiện. Với prefix bootstrap chưa hoàn tất, chỉ script có pending receipt đúng manifest mới được tiếp tục; prefix không rõ nguồn gốc phải dừng, không cài đè. Xem out/m01/bootstrap/bootstrap.log khi GNU build lỗi.

Nếu SHA tái lập khác nhau, giữ logs/JSON, kiểm tra SOURCE_DATE_EPOCH và prefix maps; không hạ tiêu chí thành chỉ compile thành công. KVM không có chỉ là WARN, không ảnh hưởng nghiệm thu build sample M01.

## Harness M02

Đọc result.json cùng serial.log, stderr.log và qmp.log trong đúng thư mục run. LAUNCH_ERROR thường do path/image/firmware không hợp lệ hoặc QEMU không gọi được; PROTOCOL_ERROR cần đối chiếu QMP JSON/capabilities/query-status. TIMEOUT là deadline hết, không suy ra kernel panic. CLEANUP_ERROR là không shutdown/reap sạch; không báo PASS chỉ vì có marker success.

Thiếu bootable image là SKIPPED/77; đường dẫn được chỉ định nhưng sai là FAIL. Emulator launch-only PASS chỉ xác minh QEMU paused và QMP, không xác minh OS boot. Kiểm tra cặp OVMF CODE/VARS và bản copy riêng, không sửa firmware hệ thống hoặc dùng disk thật.

Khi cần debug thủ công ở milestone có guest thật, sao chép command từ result.json và thêm -S cùng -gdb tcp:127.0.0.1:1234, dùng cổng riêng chưa bị chiếm; không bật GDB server mặc định trong CI. Không đánh dấu debug/boot PASS khi chưa thực thi.

Nếu Toolchain CI kẹt tải packages từ azure.archive.ubuntu.com, logs M02 đã xác nhận mirror có thể rất chậm. Hai workflows Toolchain/Harness hiện thay URL Azure bằng HTTPS archive.ubuntu.com và dùng apt timeout/retries; step setup timeout tương ứng 10/5 phút; vẫn giữ apt signature checks và package list. Đây là cấu hình CI, không đổi apt sources của host local. Không bỏ regression hoặc báo PASS khi bước dependency bị hủy.

## Contract M03

Thiếu Ninja/CMake trong PATH: activate .venv trước configure; không thay compiler target bằng GCC host. Lỗi cross xem out/m03/cross.log và cross.json; undefined symbols là FAIL cần sửa thay vì link runtime host.

LeakSanitizer báo không hoạt động dưới ptrace trong sandbox: suite M03 cấu hình detect_leaks=0 vì thư viện không heap, vẫn chạy ASan/UBSan. Không tắt memory/undefined-behavior checks để vượt lỗi. Input validation thất bại không có nghĩa firmware/OS đã chạy; xem status và đúng fixture/span. Không dereference physical addresses trong host test để khắc phục TRUNCATED ACPI span.
