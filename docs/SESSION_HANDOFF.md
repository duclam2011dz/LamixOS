# M05 — BIOS Stage2 đang kiểm chứng, 2026-10-28

Trạng thái milestone: M00=DONE; M01=DONE; M02=DONE; M03=DONE; M04=DONE; M05=VERIFYING

Người dùng duyệt triển khai kế hoạch M05 trong Default mode. Không triển khai M06/M07/M08, không có tag M05 hoặc quyền tự merge/Release. Đây là BIOS→ELF kernel probe, chưa có kernel sản phẩm.

## Git và phạm vi

Nhánh feat/m05-bios-stage2, base main/origin `ab2d1e524919f838dce6fbc5412fc70b58274562`. SHA hiện hành lấy git rev-parse HEAD; chưa ghi SHA của chính handoff qua vòng lặp. M04 tag object90778d4b630c9fe5292f5d43c49cd6a91b8670b7 giữ peeled1c2f001896327d06a3875fa5b7443f847ae2ad7f; tags M00–M04 không đổi.

Stage2 bootstrap32 KiB + core C32, BIOS thunk/E820/A20, GPT/FAT32/ELF parser thuần, allocator/page tables/LBP và probe64. Audit [BIOS_STAGE2](BIOS_STAGE2.md), [kế hoạch](../PLANS.md), lệnh [TESTING](TESTING.md#bios-stage2-m05). Stage1 source/hash/440 và LBP/GPT M03 giữ nguyên. Fixture formatter chỉ tests, không mount/device/overwrite hoặc generator sản phẩm.

## Bằng chứng hiện có

- Gate compile/link32/64: PASS; LBP alignment32 bằng -malign-double; helpers không runtime host/libgcc64, Stage1 hash a63736f43692aa428de888e6fd10979f098be29084fa79a8e4ef7ca1449296c4.
- Debug build: core44528 byte file/176128 memory; Release37128/172032, bootstrap32768 cả hai, dưới gate256 KiB. Size mới phải lấy build.json sau source changes.
- M05 Debug/Release: CTest4/4, host45 và guest51 PASS mỗi profile. Guest positives7 và expected PANIC44; RAM64/256/4608, no-NX, fragmentation/active FAT; negatives/EIO/RO/guard/injections đúng reason, cleanup và protected hashes.
- Host ASan/UBSan: CTest1/1 PASS. Regression PASS: host251/122 qemu deselected, validator/33 unittest, M03 CTest4/4/sanitizer3/3/cross, M04 suites2/2 và M02 fixture/paused launches. OS skeleton2 cases SKIPPED; M01 local không chạy lại. Chưa coi CI hoặc nghiệm thu PASS.
- Reports ignored out/m05 có XML, per-run JSON/logs và layout.json. Gate-only ELF không dùng vào boot fixture. Negative guest vẫn PANIC, không đổi thành guest PASS.

Các lỗi trung gian đã sửa: thiếu Ninja khi chưa activate venv, thiếu helper Debug, higher-half code model/link, fixture note sửa nhầm rodata và typo GUID host oracle phát hiện. Logs guest lỗi còn trong out/m05; compiler logs trong build mô tả build cuối. Source/artifact hash guard chặn chạy guest với binary cũ. Không dùng failed runs làm PASS.

## Giới hạn và next exact action

CI M05 PENDING, chưa push/PR trong snapshot handoff này. Bốn workflows baseline M04 đã SUCCESS trên ab2d1e5, không dùng thay CI SHA M05. Tiếp theo: hoàn tất regression/review, commit/push branch và mở PR; xác minh bốn workflows đúng SHA, cập nhật handoff theo kết quả rồi chờ người dùng review/xác nhận hoàn thành riêng.

OS boot skeleton BIOS/UEFI vẫn SKIPPED vì chưa có image sản phẩm mặc định. UEFI M06/parity M07/kernel M08, PC thật/4Kn/EDD không hỗ trợ/firmware vi phạm ABI NOT RUN. E820/A20 failure cases là adapters test-only. Không bắt đầu milestone tiếp theo, không tạo tag M05/Release hoặc tự merge.
