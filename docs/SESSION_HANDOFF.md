# M04 — BIOS Stage1 đang kiểm chứng, 2026-10-27

Trạng thái milestone: M00=DONE; M01=DONE; M02=DONE; M03=DONE; M04=VERIFYING

Người dùng đã duyệt Stage1 và vòng tối ưu giữ checks/ABI/gate, cho phép tiếp tục M04 sau gate PASS. M04 chưa DONE, chưa có tag/merge. Không bắt đầu M05. Đây là bootloader smoke, chưa có kernel hoặc image OS.

## Git và phạm vi

Nhánh feat/m04-bios-stage1, base main/origin `6d05b818636fdfef9e1f1d8aaa2e8dc5deedfa0e`. Các thay đổi M04 đã local PASS, đang chuẩn bị review/commit; SHA thật lấy bằng git rev-parse HEAD, không tự ghi SHA của chính handoff qua vòng lặp. Tags M00–M03 giữ nguyên; milestone/M03 giữ ab7b1e4, object 138293cedea692c4a86c9e493f982182cc81b2c9.

Thêm Stage1 NASM/probe real-mode/test builder sparse 8 GiB, CTest host/guest, audit [BIOS_STAGE1](BIOS_STAGE1.md); harness purpose bios-stage1/fault injection/TMPDIR per-run; metadata encoder M03 tách dùng lại; validator trạng thái M04 và Harness CI mở rộng. Không đổi code LBP/CMake sample M01 hoặc filesystem/GPT product profile. Xem [kế hoạch](../PLANS.md) và [TESTING](TESTING.md#bios-stage1-m04).

## Gate và kiểm chứng local

Baseline 539 byte bị assembler chặn, chưa từng boot. Vòng A 494→B 467→C 436; binary 440 byte, padding 4. Sửa DL sau UART, bố trí error branches, REP full comparisons, EBP giữ capacity, DAP stack căn 4 và message data riêng. Không bỏ checks hoặc dùng external helper/self-modifying code. Baseline/group source/listings, logs và gate.json ở out/m04 ignored; listing cuối ở build/m04/stage1.lst.

| Kiểm tra đã chạy | Kết quả |
|---|---|
| Gate NASM và review tĩnh | PASS 436/440 byte; binary 440; matrix source ở BIOS_STAGE1 |
| M04 CTest | PASS 2 suites; 15 host tests và 69 guest cases (3 positives, 66 expected PANIC negatives) |
| Metadata/payload/probe | Header 32 bytes, GUID 16 bytes, high/bounds/window/trailer errors đúng reason |
| QEMU EIO thật | PASS negative case, blkdebug sector payload+31 nhận R, không success marker |
| Protected extents/firmware/cleanup | PASS; snapshot, hashes và no forced cleanup/reaped group |
| Host regression | PASS 203 tests, 71 qemu cases deselected |
| Validator/unittest | PASS validator và 30 tests |
| M01 | PASS Debug/Release/ELF/provenance và SHA-256 giữa hai source roots |
| M03 | PASS CTest 4/4, native ASan/UBSan 3/3 và cross compile-only |
| M02 | PASS 33 fixtures, 2 BIOS/UEFI paused launches; 2 OS skeleton SKIPPED/77 |
| OS boot BIOS/UEFI | SKIPPED/77, không có image OS mặc định |
| Unsupported EDD/4Kn/DL ngoài profile, BIOS sai ABI, CF=0 short count giả lập, GDB, kernel/UEFI/A-B boot | NOT RUN |

Smoke đầu EXIT 1 vì snapshot mặc định không tạo được temporary filename trên /var/tmp read-only; đã sửa TMPDIR chỉ trong environment tiến trình con sang đúng run directory, vẫn giữ snapshot. Giữ logs failure cũ, không tính nó là guest PASS. Real runs sau sửa dùng default command builder nên backend=qemu; custom host factories vẫn backend=fixture.

out/m04 có host.xml/guest.xml/ctest.xml/regression.xml, binary.json/gate.json, serial/QMP/stderr/result.json/case.json từng run; case.json ghi expected/actual/reason và hashes trước/sau. Negative testcase PASS nghĩa nhận PANIC dự kiến, không đổi result guest thành PASS. Image/probe/firmware không upload.

## CI và next exact action

M04 CI hiện NOT RUN cho thay đổi chưa push; không dùng CI M03 làm chứng nhận M04. Harness CI sẽ build NASM/CTest M04 và upload m04-reports, giữ ba workflows khác. Cần bốn workflows SUCCESS trên đúng head SHA và PR review trước merge; tag M04 cần người dùng xác nhận hoàn thành riêng.

Review diff/staged/secrets, validator/unittest trước commit; commit/push nhánh, mở PR và kiểm tra bốn workflows. Cập nhật handoff bằng bằng chứng thật, rồi chờ review/nghiệm thu. Không tự merge/tag/M05 hoặc tạo version Release. Gate trở lại FAIL phải dừng theo kế hoạch, không hạ checks.
