# M05 — BIOS Stage2 chờ review, 2026-10-28

Trạng thái milestone: M00=DONE; M01=DONE; M02=DONE; M03=DONE; M04=DONE; M05=VERIFYING

Người dùng duyệt triển khai kế hoạch M05 trong Default mode. Không triển khai M06/M07/M08, không có tag M05 hoặc quyền tự merge/Release. Đây là BIOS→ELF kernel probe, chưa có kernel sản phẩm.

## Git và phạm vi

Nhánh feat/m05-bios-stage2, base main/origin `ab2d1e524919f838dce6fbc5412fc70b58274562`. Commit code đã push: `68cbd480d42112cc9a6ac8dba8024f2b304e6142`, [PR #3](https://github.com/duclam2011dz/LamixOS/pull/3) open, chưa merge. Bản handoff này thuộc lượt sửa bootstrap tiếp sau; SHA HEAD thực lấy git rev-parse HEAD, không ghi SHA của chính file qua vòng lặp. M04 tag object90778d4b630c9fe5292f5d43c49cd6a91b8670b7 giữ peeled1c2f001896327d06a3875fa5b7443f847ae2ad7f; tags M00–M04 không đổi.

Stage2 bootstrap32 KiB + core C32, BIOS thunk/E820/A20, GPT/FAT32/ELF parser thuần, allocator/page tables/LBP và probe64. Audit [BIOS_STAGE2](BIOS_STAGE2.md), [kế hoạch](../PLANS.md), lệnh [TESTING](TESTING.md#bios-stage2-m05). Stage1 source/hash/440 và LBP/GPT M03 giữ nguyên. Fixture formatter chỉ tests, không mount/device/overwrite hoặc generator sản phẩm.

## Bằng chứng hiện có

- Gate compile/link32/64: PASS; LBP alignment32 bằng -malign-double; helpers không runtime host/libgcc64, Stage1 hash a63736f43692aa428de888e6fd10979f098be29084fa79a8e4ef7ca1449296c4.
- Debug build: core44528 byte file/176128 memory; Release37128/172032, bootstrap32768 cả hai, dưới gate256 KiB. Size mới phải lấy build.json sau source changes.
- M05 Debug/Release: CTest4/4, host46 và guest54 PASS mỗi profile. Guest positives9 và expected PANIC45; RAM64/256/4608, no-NX, fragmentation/active FAT; negatives/EIO/RO/guard/injections đúng reason, cleanup và protected hashes.
- Host ASan/UBSan: CTest1/1 PASS. Regression PASS: host252/125 qemu deselected, validator/33 unittest, M03 CTest4/4/sanitizer3/3/cross, M04 suites2/2 và M02 fixture/paused launches. OS skeleton2 cases SKIPPED; M01 local không chạy lại. Toolchain CI đã PASS bao gồm M01 reproducibility, M03 cross và M05 guest. Chưa xác nhận DONE/nghiệm thu riêng của người dùng.
- Reports ignored out/m05 có XML, per-run JSON/logs và layout.json. Gate-only ELF không dùng vào boot fixture. Negative guest vẫn PANIC, không đổi thành guest PASS.

Các lỗi trung gian đã sửa: thiếu Ninja khi chưa activate venv, thiếu helper Debug, higher-half code model/link, fixture note sửa nhầm rodata và typo GUID host oracle phát hiện. Logs guest lỗi còn trong out/m05; compiler logs trong build mô tả build cuối. Source/artifact hash guard chặn chạy guest với binary cũ. Không dùng failed runs làm PASS.

## Sửa sau review segment bounds

CI68cbd48 từng PASS nhưng review phát hiện real-mode physical offset32 có thể phụ thuộc hidden segment state; đã chuyển CRC/BSS/map copy sang segment:offset chuẩn, giữ checks/ABI/size. Thêm inject-segments (limits64 KiB tường minh), large-core70000 bytes và corruption cuối file; không dùng CI cũ để nghiệm thu HEAD sửa. Cần xác minh CI HEAD sau push.

## Giới hạn và next exact action

Bốn workflows SUCCESS trên đúng code SHA `68cbd480d42112cc9a6ac8dba8024f2b304e6142`:

- [Documentation CI](https://github.com/duclam2011dz/LamixOS/actions/runs/40689060125).
- [Boot Contract CI](https://github.com/duclam2011dz/LamixOS/actions/runs/40689060116).
- [Harness CI](https://github.com/duclam2011dz/LamixOS/actions/runs/40689060041).
- [Toolchain CI](https://github.com/duclam2011dz/LamixOS/actions/runs/40689060120).

Đã đọc ZIP m05-host-reports id12815524795: host45/45, native Debug/Release và ASan/UBSan PASS. Đã đọc ZIP m05-target-reports id12816865335: 546 report files, JUnit host46 và guest54 mỗi profile, 102 case.json/result.json (14 PASS/88 PANIC đúng expected). Hashes binary/source của Debug/Release khớp local; protected hashes/firmware/cleanup đúng, EIO payload nhận DISK; không có image/firmware/binary trong ZIP. Audit ignored out/m05/ci-artifact-audit.json.

Sau push commit tài liệu này phải xác minh lại bốn CI theo HEAD thực; không dùng CI code SHA làm PASS cho HEAD mới. Next exact action: chờ review PR #3 và người dùng xác nhận hoàn thành riêng trước DONE/merge/tag. Không còn code đang dở được biết.

OS boot skeleton BIOS/UEFI vẫn SKIPPED vì chưa có image sản phẩm mặc định. UEFI M06/parity M07/kernel M08, PC thật/4Kn/EDD không hỗ trợ/firmware vi phạm ABI NOT RUN. E820/A20 failure cases là adapters test-only. Không bắt đầu milestone tiếp theo, không tạo tag M05/Release hoặc tự merge.
