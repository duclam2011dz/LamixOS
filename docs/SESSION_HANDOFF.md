# Bàn giao đóng M04 — 2026-10-27

Trạng thái milestone: M00=DONE; M01=DONE; M02=DONE; M03=DONE; M04=DONE

Người dùng đã xác nhận hoàn thành và duyệt tag/merge/bàn giao. M05 vẫn PLANNED, chưa được phép bắt đầu trong lượt đóng này. M04 là BIOS bootloader smoke, chưa chứng minh Lamix OS boot.

## Git và phạm vi

- Commit nghiệm thu: `1c2f001896327d06a3875fa5b7443f847ae2ad7f`; code M04 tại `72df89001b3787067fd27cb9dfc9c1bde8c1ab40`.
- Annotated `milestone/M04`: tag object `90778d4b630c9fe5292f5d43c49cd6a91b8670b7`, peeled commit đúng SHA nghiệm thu; đã push. Không di chuyển sang commit tài liệu.
- [PR #2](https://github.com/duclam2011dz/LamixOS/pull/2) đã merge, merge commit `78b5a60e54c5cdfb32f1ca32a4d4a42ece7fa78d`. Tags M00–M03 giữ nguyên.
- Nhánh hiện hành main/origin; commit đóng thủ tục tiếp sau chỉ sửa tài liệu. Lấy HEAD thực bằng git rev-parse HEAD, không tạo vòng lặp ghi SHA của chính handoff.

Stage1/probe, sparse fixtures, harness bios-stage1 và CI theo [kế hoạch](../PLANS.md), [audit](BIOS_STAGE1.md), [TESTING](TESTING.md#bios-stage1-m04). Không đổi LBP/GPT M03 hoặc triển khai Stage2/kernel/UEFI loader.

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

Bốn workflows SUCCESS trên đúng SHA nghiệm thu `1c2f001896327d06a3875fa5b7443f847ae2ad7f`:

- [Documentation CI](https://github.com/duclam2011dz/LamixOS/actions/runs/40525737668).
- [Boot Contract CI](https://github.com/duclam2011dz/LamixOS/actions/runs/40525737404).
- [Toolchain CI](https://github.com/duclam2011dz/LamixOS/actions/runs/40525737717).
- [Harness CI](https://github.com/duclam2011dz/LamixOS/actions/runs/40525737687).

Artifact m04-reports id 12743517058 của code 72df890 đã được đọc: host15/guest69, binary436/440/padding4, 3 PASS/66 PANIC đúng expected, hashes và cleanup đúng; không có image/firmware/binary trong ZIP.

Lượt đóng chỉ chạy lại validator và 30 unittest tài liệu, không chạy lại toàn bộ build/QEMU local vì không sửa code. CI commit đóng thủ tục phải kiểm tra theo HEAD thực tế sau push; kết quả nghiệm thu không thay thế CI của SHA mới.

Next exact action: kết thúc M04; chỉ khảo sát/lập kế hoạch M05 khi người dùng yêu cầu và bật Plan Mode thật. Không có code đang dở được biết. Không tự tạo version tag, Release hoặc bắt đầu M05.
