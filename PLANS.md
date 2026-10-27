# Kế hoạch hiện hành — M03

Trạng thái milestone: M00=DONE; M01=DONE; M02=DONE; M03=DONE

Người dùng duyệt kế hoạch M03 và chuyển sang chế độ triển khai ngày 2026-10-26. M00–M02 giữ nguyên annotated tags. Không bắt đầu M04.

## Phạm vi đã duyệt

LBP v1 little-endian, header 64 byte + records căn 8; ELF PT_NOTE negotiation; memory normalization E820/UEFI; higher-half entry với allocations ban đầu dưới 4 GiB; ownership/release conditions. Thư viện C11 freestanding dùng caller-owned buffers, không heap/firmware/disk I/O. Headers dùng được từ C++17 và có assertions từng field.

GPT Protective MBR + BIOS Boot + ESP + System A/B + User Data; profile 8 GiB, alignment 1 MiB, boot 512 byte, mô hình/tests 512/4096. ESP boot bundles theo slot; chưa làm update/rollback M39. Đặc tả tại [BOOT_PROTOCOL](docs/BOOT_PROTOCOL.md), [layout LBP](docs/LBP_LAYOUT.md) và [IMAGE_LAYOUT](docs/IMAGE_LAYOUT.md).

## Thứ tự và kiểm chứng

- [x] Đặc tả/headers/ADR và các quyết định đã duyệt thống nhất.
- [x] Parser/note/record iteration, normalization thuần và host GPT model.
- [x] Host Debug/Release, ASan/UBSan, cross compile-only và assertions PASS.
- [x] Validator/unittest M03, regression M01 và harness M02 PASS; boot cases SKIPPED.
- [x] Review diff/staged và bốn workflows SUCCESS trên commit code cuối 66f5b1e; artifact JUnit/layout đã đối chiếu, xem handoff.
- [x] Bốn workflows SUCCESS trên SHA nghiệm thu ab7b1e4; người dùng phê duyệt kết thúc/merge/tag ngày 2026-10-27, PR #1 đã merge và tag đã xác minh.

Bằng chứng cập nhật tại [handoff](docs/SESSION_HANDOFF.md). Local PASS không chứng minh CI hoặc OS boot. Không tạo bootloader/kernel/image generator; không ghi disk thật.

## Rollback và gate

Rollback bằng commit tiếp nối/revert đúng diff M03, giữ thay đổi người dùng và logs. Không reset/clean hàng loạt hoặc đổi tags M00–M02. Đổi ABI/mapping/layout ngoài kế hoạch phải xin duyệt bổ sung. Chỉ tạo tag M03 sau khi đủ nghiệm thu và người dùng xác nhận hoàn thành riêng; không tự Release hoặc sang M04.

## Đóng thủ tục — 2026-10-27

Annotated `milestone/M03` giữ `ab7b1e4bdab199f1368d207a2deff79806bc1433`, không chuyển sang merge commit hoặc commit tài liệu. PR #1 merge bằng commit `347cc346edc8739feb15fbb38c3b90994d9941a6`. Lượt đóng chỉ cập nhật README/ROADMAP/PLANS/CHANGELOG/handoff, validator và unittest phải PASS; kiểm tra bốn workflows của HEAD tài liệu sau push. Không chạy lại bootstrap hoặc sửa code/workflows.

M03 DONE theo xác nhận người dùng; không bắt đầu M04 trong lượt này. Chờ chỉ dẫn mới; milestone tiếp theo cần Plan Mode thật và kế hoạch riêng. Không tạo version tag hoặc GitHub Release.
