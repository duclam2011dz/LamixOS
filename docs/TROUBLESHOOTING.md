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

## BIOS Stage1 M04

Nếu NASM báo TIMES value âm, gate 440 byte FAIL; không xóa assertion hoặc viết qua PMBR metadata. Giữ listing/source hash và dừng theo gate/plan hiện hành. Vòng tối ưu539→436 byte đã giữ ở evidence local; xem BIOS_STAGE1 cho check matrix.

QEMU snapshot báo Could not get temporary filename/Read-only file system: harness dùng TMPDIR riêng trong run directory. Không tắt snapshot hoặc đổi quyền /var/tmp. Guest M04:FAIL có reason D/G/N/R; probe dùng P. Expected PANIC ở negative tests không phải lỗi harness nếu reason và cleanup khớp, cũng không phải OS boot PASS. Đọc case.json cùng result.json/serial.log.

## Stage2 M05

- Không tìm Ninja: activate .venv trước CMake; không cài lại dependencies đang có. Build FAIL phải sửa trước CTest; build.json/hash guard chặn dùng artifact cũ.
- C32 LBP alignment assertions: kiểm mọi translation unit có -m32 -malign-double; không sửa asserts/wire structs để tránh lỗi. Undefined __umoddi3 ở Debug là compiler helper remainder, không link libgcc64.
- ELF64 higher-half relocation truncated: dùng -mcmodel=kernel cho code64; p_paddr phải zero theo profile sau link producer, không tắt linker checks hoặc sửa virtual addresses.
- M05:FAIL có reason group; EXCEPTION/vector0e ở RO/guard negatives là expected PANIC. HANDOFF không phải PASS. EIO phải nhận DISK; malformed FAT/GPT/ELF không có I/O fault nhận đúng nhóm parser.
- Headers/core CRC sai không chạy core. GPT hỏng không repair/fallback; slot A lỗi không tự boot B. Unsupported required ACPI/framebuffer/NX phải bị từ chối đúng note policy.
- Fixture note mutations phải tìm PT_NOTE envelope; ELF có thể chứa chuỗi LAMIXREQ khác trong rodata validator. Không tìm chuỗi đầu tiên để suy ra descriptor.

Audit/limits: [BIOS_STAGE2](BIOS_STAGE2.md). Không debug bằng cách bỏ snapshot, RWX mappings, guard hoặc validation; giữ logs failure cũ, phân biệt adapter với lỗi firmware thật.

## EFI loader M06

- Build thiếu `clang`, `lld-link` hoặc `nasm`: kiểm tra PATH; script không cài dependencies. Clang target PE/COFF riêng với GCC cross ELF. LLD unresolved symbol là thiếu runtime freestanding, không thêm CRT mặc định.
- `GPT_IDENTITY`/`ESP_DEVICE_PATH`: loader chỉ chấp nhận GPT profile Lamix và phải khớp ESP HD node với whole-disk Block I/O path; không hard-code thiết bị hoặc mở slot B.
- `ELF`: kiểm `build/m06/build.json`, source hashes và note `LAMIXREQ`. Feature required không có record hợp lệ phải bị từ chối; ACPI RSDP lệch alignment/checksum thì optional feature bị bỏ.
- Nếu OVMF guest dừng sau `TABLES_OK`, xem `serial.log`/`case.json`; không coi PE build hoặc QEMU launch là boot thành công. `M06:PASS` chỉ do cùng kernel probe phát sau khi kiểm LBP/page tables.
- `ExitBootServices()` chỉ retry một lần khi `EFI_INVALID_PARAMETER`; không gọi protocol, cấp phát hoặc ghi serial sau lần GetMemoryMap cuối. Buffer growth có một lần bounded, tối đa 2 MiB; vượt mức fail-stop.
- OVMF CODE gốc read-only, mỗi run dùng VARS riêng; disk snapshot bật. Hash thay đổi hoặc cleanup bị cưỡng bức là FAIL. OVMF PASS không xác nhận PC thật, 4Kn, Secure Boot hay mọi GOP/ACPI firmware.

## VMM/kernel address space M10

- Nếu dừng trước `M10:VMM root=switched`, xem `M10:FAIL` và panic frame; kiểm tra LBP kernel segments, BootInfo reservation, stack/guard, TABLES reservation và PMM còn usable frame. Không giả định mọi physical address đang identity-map.
- `#PF` tại scratch VA `0xfffffd0000000000` thường chỉ ra scratch PTE/control alias chưa được dựng hoặc page-table frame bị tái sử dụng. Trước khi đổi CR3, scratch access dựa vào TABLES cũ; sau đổi CR3 chỉ được sửa PTE qua control alias.
- Fault profiles có expected errors: ghi trang RO supervisor `error=3`; execute NX `error=0x11`; guard write `error=2`; mọi trường hợp phải có vector `0e` và CR2 phù hợp. `TIMEOUT`, triple fault, hoặc chỉ thấy QEMU launch không phải PASS.
- Không bỏ snapshot hoặc tắt W^X/NX/guard để vượt test. Cùng kernel chạy SeaBIOS/OVMF; PC thật và CPU không có NX chưa được kiểm chứng.
