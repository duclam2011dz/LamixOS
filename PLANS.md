# Kế hoạch hiện hành — M05

Trạng thái milestone: M00=DONE; M01=DONE; M02=DONE; M03=DONE; M04=DONE; M05=DONE

Người dùng đã duyệt triển khai kế hoạch M05 trong phiên Default ngày 2026-10-28. M04 đã đóng; không triển khai M06/M07/M08.

## Contract và phạm vi đã duyệt

BIOS Stage1 nguyên source/hash/ABI 440 byte. Stage2 bootstrap 32 KiB tại 0x10000, core C32 tại 0x20000, toàn memory core tối đa 256 KiB. Header LAMIXS2 có CRC32, bounded loading; E820/A20, BIOS thunk 32↔16. Cross GCC -m32 -malign-double, không runtime host/libgcc64.

GPT được kiểm cả hai bản/CRC/overlap; ESP FAT32 chỉ đọc, kernel-only EFI/LAMIX/A/KERNEL.ELF, không fallback B. ELF64 ET_EXEC higher-half, p_paddr=0, loader cấp physical từ USABLE dưới 4 GiB. LBP v1 không đổi; đúng năm records bắt buộc; serial/NX, chưa ACPI/framebuffer/modules. Paging 4 KiB, W^X/RO/guard, kernel probe dùng lại M06.

## Gates theo thứ tự

- [x] C32/linker/helpers và probe ELF64: assertions LBP32/64, không undefined/runtime, Stage1 hash nguyên.
- [x] Bootstrap/header/core, E820/A20 và BIOS thunk: budget32/256 KiB, CRC và read failures.
- [x] GPT/FAT32/ELF/allocation/BootInfo: host Debug/Release/sanitizer.
- [x] Paging/transition/higher-half entry: probe kiểm contract thật.
- [x] Guest positive/negative/EIO/fault probes và regression liên quan.
- [x] Bốn CI SUCCESS trên code0a0f496; đã đọc/đối chiếu host/target artifacts và cập nhật tài liệu.
- [x] Người dùng xác nhận hoàn thành và duyệt tag/merge/bàn giao; annotated milestone/M05 trỏ4742e2d đã push, PR #3 merge tại c05b0b2.

## Kiểm thử và rollback

Suite độc lập tests/bios_stage2; host và guest tách marker. RAM64/256 MiB/>4 GiB, NX/no-NX, fragmentation, malformed metadata/ELF/note, EIO, RO/guard faults, cleanup và protected hashes. Giữ bốn workflows; Boot Contract CI chạy host/sanitizers, Toolchain CI build/guest cross, Harness giữ M02/M04. Không tự chạy toàn bộ M01 local.

Fixtures file mới sparse8 GiB, FAT32 test-only, không mount/device/overwrite; generator sản phẩm M07 chưa triển khai. Lưu reports ignored out/m05; không upload images/firmware. Chi tiết kết quả thật tại [handoff](docs/SESSION_HANDOFF.md).

Nếu vượt size, thiếu RAM bootstrap, không link ABI32 đúng hoặc cần đổi contract/profile thì dừng phần phụ thuộc, trình kế hoạch bổ sung; không giảm checks. Rollback bằng diff/commit tiếp nối; không reset/clean hoặc đổi tags M00–M04.

## Kết quả trước PR

Local PASS: Debug/Release CTest4/4 (host46, guest54 mỗi profile), native ASan/UBSan1/1, global host252/125 qemu deselected, validator/33 unittest; M03 native4/4/sanitizer3/3/cross, M04 suites2/2 và M02 fixtures/paused launches. OS skeleton2 cases SKIPPED. Bốn CI code0a0f496 SUCCESS; [PR #3](https://github.com/duclam2011dz/LamixOS/pull/3) đã merge theo phê duyệt riêng. CI HEAD tài liệu cuối phải xác minh theo SHA thực, không lặp commit để tự ghi SHA handoff.

Review sau CI đầu phát hiện real-mode offset32 vượt64 KiB; sửa trong cùng contract bằng segment:offset chuẩn. Bổ sung strict-limit/cross-segment fixtures; CI revision sửa0a0f496 đã SUCCESS, artifact108 guests/hashes/cleanup đã đọc; CI của HEAD tài liệu mới phải xác minh theo SHA thực.
