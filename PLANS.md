# Kế hoạch hiện hành — M04

Trạng thái milestone: M00=DONE; M01=DONE; M02=DONE; M03=DONE; M04=VERIFYING

Kế hoạch Stage1 và vòng tối ưu A/B/C đã được người dùng duyệt. Sau gate và review tĩnh PASS, được tiếp tục probe/fixture/guest/CI M04. Không mở rộng sang M05.

## Contract giữ nguyên

Mã/data PMBR<=440 byte, EDD 512/disk<2 TiB, không CHS hoặc truncate. Primary GPT/header shape theo M03, GUID 16 byte/128 entries, bounds và 64-sector window; retry 3/partial-read rejection. COM1 bounded polling, diagnostics D/G/N/R và handoff real-mode đúng ABI. Không helper ngoài PMBR, code/data overlap, self-modifying code, disk write hoặc đổi LBP/GPT.

Gate ban đầu FAIL 539 byte, đã dừng. Vòng tối ưu đo A 494, B 467, C 436 byte; binary 440 với 4 byte padding. Baseline/group source/listings/logs ignored tại out/m04; source hash và binary.json/gate.json là bằng chứng, không suy ra guest correctness từ size. Audit từng invariant tại [BIOS_STAGE1](docs/BIOS_STAGE1.md).

## Tiến độ và nghiệm thu

- [x] Lưu baseline 539 và đo ba nhóm A/B/C, giữ assertion budget.
- [x] Review REP pointer/counter, EBP capacity lifetime, DAP 16 byte trên stack căn 4, flags và stack cleanup; sửa DL sau UART.
- [x] Stage1/probe và sparse test-only GPT fixtures; không format filesystem hoặc tạo image OS.
- [x] Local guest 69 cases PASS gồm relocation/exact-window, header/GUID/bounds, payload/trailer và EIO thật; backend=qemu/mode=bios-stage1, negative status không đổi thành guest PASS.
- [x] Thêm host tests và harness purpose/fault/TMPDIR, giữ positional Config/default modes; mở rộng trạng thái validator M04 và Harness CI.
- [x] Regression local PASS: 203 host tests, validator/30 unittest, M01 reproducibility, M02/M03 và M04 suites.
- [x] Review staged/secrets và commit/push code 72df890; mở PR #2, artifact CI đã đọc.
- [x] Bốn workflows SUCCESS trên code 72df890, binary size/hash và 69 guest results đúng expected.
- [ ] CI HEAD tài liệu cuối, PR review và người dùng xác nhận hoàn thành riêng trước DONE/tag.

Commands và reports theo [TESTING](docs/TESTING.md#bios-stage1-m04), kết quả cuối tại [handoff](docs/SESSION_HANDOFF.md). Gate FAIL sau vòng này phải dừng xin kế hoạch, không hạ checks. Guest fixes trong contract phải qua gate lại trước test.

## Ranh giới và rollback

Fixtures chỉ metadata/Stage1/probe/sentinels trong sparse image test; snapshot và protected extent hashes bắt buộc. Full guest GPT CRC/backup, E820/A20, CPU mode transition, ELF/kernel/LBP handoff, UEFI và generator M07 ngoài phạm vi. OS boot skeleton vẫn SKIPPED; không suy ra tương thích PC thật từ SeaBIOS.

Rollback theo đúng diff/file task, giữ source/logs và chỉnh sửa người dùng; không reset/clean hàng loạt, đổi tags M00–M03 hoặc ghi disk thật. Không tự merge/tag/Release/M05. Cuối lượt cập nhật trạng thái và CI đúng bằng chứng, không lặp commit để tự ghi SHA của handoff.
