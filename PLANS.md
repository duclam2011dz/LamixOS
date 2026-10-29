# Kế hoạch hiện hành — M06

Trạng thái milestone: M00=DONE; M01=DONE; M02=DONE; M03=DONE; M04=DONE; M05=DONE; M06=VERIFYING

Người dùng duyệt triển khai M06 ngày 2026-10-28. Phạm vi đã làm local: EFI application PE/COFF x64 bằng Clang+LLD; UEFI filesystem/GPT device identity, ELF load chung, GOP/ACPI optional, LBP v1 và handoff qua ExitBootServices; guest OVMF dùng cùng kernel probe M05. Chưa commit/push/tag/merge; CI trên SHA chứa thay đổi còn PENDING. Vì vậy M06 chưa DONE và không bắt đầu M07.

## Contract được giữ

- `LBP v1`, BIOS Stage1 440 byte và Stage2 budget/ABI giữ nguyên. ELF ET_EXEC higher-half, `p_paddr=0`; kernel allocations vật lý dưới 4 GiB. EFI image/trampoline không giả định cùng giới hạn; trampoline/stack/tables được cấp riêng.
- Firmware functions dùng Microsoft x64 ABI; kernel entry dùng System V theo LBP. Trampoline chuyển CR3/stack/registers; LA57 bật bị từ chối.
- ESP xác định từ Loaded Image DeviceHandle, Device Path, Block I/O và GPT; đọc `EFI/LAMIX/A/KERNEL.ELF`, không fallback B.
- LBP v1 hiện giữ nguyên wire layout. UEFI phát framebuffer identity mapping RW/NX khi GOP mode tương thích; ACPI chỉ khi RSDP rev0/2, checksum/span/alignment hợp lệ. OVMF local đưa RSDP không căn 16 nên optional ACPI bị bỏ; required ACPI bị từ chối.
- GetMemoryMap dùng descriptor stride/version/key; 1 MiB buffer ban đầu, một lần tăng bounded nếu BUFFER_TOO_SMALL (tối đa 2 MiB). Không log/cấp phát/protocol sau map thành công trước ExitBootServices; retry Exit tối đa một lần cho key stale. Sau exit không dùng Boot Services.

## Gates và kết quả local

- [x] PE/COFF: EFI subsystem x64, entry, imports rỗng, relocations có; Clang/LLD/NASM version, lệnh và source hashes ghi trong build report.
- [x] QEMU OVMF guest: positive cùng kernel ELF trả `M06:PASS`; negative thiếu kernel, ELF hỏng, ACPI required nhưng không có record hợp lệ, GPT identity lỗi nhận đúng failure.
- [x] LBP/probe: kernel xác nhận feature/record, identity framebuffer mapping và toàn bộ entry mappings; cùng probe BIOS/UEFI.
- [x] M06 CTest: PE, host retry/capacity policy và 5 guest cases PASS; guest positive kiểm marker GOP. BIOS M05 Debug/Release CTest 4/4; M05 host sanitizer 1/1; M03 Debug/Release 4/4, sanitizer 3/3; M02 fixtures/launch 2 PASS, skeleton BIOS/UEFI SKIPPED; M04 host+QEMU 2/2.
- [x] GPT UEFI extent matching, framebuffer mapping above 4 GiB và mutational LBP mapping validation có host tests.
- [ ] Chạy CI trên commit/SHA chứa toàn bộ M06; review diff và quyết định nghiệm thu riêng. Chưa kiểm PC thật/4Kn.

## Kiểm thử và rollback

Lệnh chính sau khi activate `.venv`:

```bash
python3 tests/uefi/build_uefi.py --build build/m06
cmake -S tests/uefi -B build/m06-tests -G Ninja
cmake --build build/m06-tests
ctest --test-dir build/m06-tests --no-tests=error --output-on-failure
```

Suite guest tạo image mới trong temporary directory, snapshot disk, CODE read-only, VARS riêng từng run, serial/QMP/deadline/cleanup và hash bảo vệ image/firmware. Báo cáo nằm `out/m06/`; không upload firmware hoặc images. OVMF execution là PASS riêng; OS boot skeleton vẫn SKIPPED vì chưa có image sản phẩm. QEMU không đại diện PC thật.

Nếu cần thay LBP wire layout, kernel ABI, BIOS gate/budget, policy A/B hoặc scope M07/M08 thì dừng và xin duyệt kế hoạch bổ sung. Rollback bằng commit revert sau khi có commit; không reset/clean, force-push hoặc di chuyển tags. Không tạo/push `milestone/M06` cho đến khi đủ CI/review và người dùng xác nhận hoàn thành riêng.
