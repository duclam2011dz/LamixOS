# Lịch sử thay đổi

Theo dạng Keep a Changelog đơn giản. Chỉ ghi thay đổi thực tế; chưa có release.

## Unreleased

### Thêm

- Hướng dẫn Codex, giới thiệu, mục tiêu, kế hoạch M00 và quy trình đóng góp bằng tiếng Việt.
- Hệ thống 13 tài liệu docs: phân nguồn sự thật, skeleton kỹ thuật, kiểm thử và bàn giao.
- .gitignore cho build/out và cache Python.
- Repository Git với branch main.
- Skill docs-consistency tại .agents/skills/docs-consistency/SKILL.md, đã qua validator và rà soát M00.

- Giấy phép GPL-3.0-only, văn bản GPL v3 tiếng Anh chính thức.
- Validator Python độc lập, 22 unittest và workflow Documentation CI; đã chạy thành công trên GitHub-hosted runner.
- Repository GitHub public duclam2011dz/LamixOS, remote origin/main và commit bằng email noreply.
- Quy trình Git/CI, milestone tags và release gate; chuẩn hóa đường dẫn host trong tài liệu.

### Ghi chú

M00 đạt nghiệm thu phần GitHub/GPL/CI ngày 2026-10-24; người dùng đã chính thức xác nhận hoàn thành. Annotated tag milestone/M00 đã được tạo và push trên commit aa6aeb9ff8ea4620227020ea43e03f430328d6b6; chưa tạo version tag hoặc Release. Chưa có mã OS hoặc bản phát hành; M01 đạt kiểm chứng local và Toolchain CI cho build sample; annotated tag milestone/M01 đã được người dùng duyệt, tạo và push trên commit nghiệm thu 99f7591046c843d77362eed5f16cf6a34a085bbb. Không tạo version tag hoặc Release; tại mốc đóng M01 chưa bắt đầu M02.

### Thêm trong M01

- Doctor host/cross-toolchain với báo cáo JSON, dependencies có phiên bản/hash trong môi trường riêng.
- CMake/Ninja presets Debug/Release, sample NASM/C11/C++17 freestanding và verifier ELF/link provenance.
- Host tests và kiểm chứng SHA-256 giữa hai source roots; bootstrap GCC 12.2.0/Binutils 2.39 từ GNU archives đã xác minh chữ ký/hash.
- Workflow Toolchain CI đã chạy GNU source bootstrap và doctor/build/reproducibility thành công; cache chính xác và logs lỗi được cấu hình. URLs/kết quả tại handoff.

### Thêm trong M02

- Harness host QMP/serial với monotonic deadlines, phân loại kết quả, process-group cleanup và logs/JSON riêng từng run.
- Fixture tests và QEMU launch-only thật BIOS/UEFI; boot skeleton chưa có image được SKIPPED/77, không phải OS boot PASS.
- CTest host-only, pytest marker qemu, validator trạng thái mở rộng tới M02 và Harness CI riêng với JUnit/summary/logs lỗi.

M02 đã kiểm chứng local và cả ba workflows trên GitHub; kết quả/URLs ở handoff. Người dùng đã phê duyệt kết thúc; annotated tag milestone/M02 đã được tạo/push và xác minh trên commit nghiệm thu 3a4f6206f04287d07b8bd9f29d5cc941af2f50dc. Không tạo version tag hoặc GitHub Release; chưa chuyển phiên hoặc bắt đầu M03. Không tạo boot code/image hoặc thay tags M00/M01.

- Sửa setup CI bị chậm tải package từ Azure Ubuntu mirror: ưu tiên HTTPS Ubuntu archive trong runner, giữ chữ ký apt/package list, thêm retry/timeout; không đổi apt sources host local hoặc hạ tiêu chí tests.

### Thêm trong M03

- LBP v1 headers C11/C++17 với assertions từng field, ELF note descriptor, record parsing/validation và memory map normalization freestanding dùng caller-owned buffers.
- Đặc tả entry/ownership/compatibility và GPT profile; mô hình host metadata512/4096, không tạo bootable image hoặc disk I/O.
- Host positive/negative/golden tests, native mutations với ASan/UBSan, cross compile-only không undefined runtime symbols; Boot Contract CI và cross step trong Toolchain CI.
- Bảo vệ cả ACPI reclaim có nonvolatile attributes khỏi cấp phát/thu hồi; bổ sung negative test.
- Validator và unittest trạng thái mở rộng đến M03, đồng bộ tài liệu API/memory/testing; giữ nguyên ba prompt gốc và tags M00–M02. Kết quả local/CI thật ở handoff; tại mốc triển khai chưa xác nhận DONE hoặc tạo tag M03.

### Đóng M03 — 2026-10-27

- Người dùng phê duyệt kết thúc M03, tạo/push tag và merge. Annotated milestone/M03 đã push, trỏ commit nghiệm thu ab7b1e4bdab199f1368d207a2deff79806bc1433 có bốn workflows SUCCESS.
- PR #1 đã merge bằng merge commit 347cc346edc8739feb15fbb38c3b90994d9941a6; giữ nguyên lịch sử và tags M00–M02. Đồng bộ trạng thái DONE và handoff; không triển khai M04, không tạo version tag hoặc GitHub Release.

### Khảo nghiệm gate M04 — 2026-10-27

- Thêm bản draft NASM Stage1 và CMake target assemble/listing để kiểm tra budget PMBR. Gate FAIL: 539/440 byte, NASM từ chối tạo binary (vượt 99 byte).
- Dừng theo gate đã duyệt; ghi BLOCKED trong kế hoạch/roadmap/handoff. Chưa thêm probe, fixture image, harness changes, CI hoặc guest tests M04; không bắt đầu M05.

### Triển khai M04 sau vòng tối ưu được duyệt

- Tối ưu Stage1 539→494→467→436 byte hữu ích, artifact 440 byte; giữ checks/ABI và assertion budget. Sửa DL boot drive sau UART init, gom REP comparisons, DAP stack căn 4 và diagnostic data riêng; không external helper hoặc self-modifying code.
- Thêm real-mode probe 32 KiB, sparse GPT test fixture, host/binary/path/reproducibility và 69 guest positives/negatives gồm blkdebug EIO thật; không tạo image OS hoặc Stage2.
- Harness purpose bios-stage1, fault Config test-only và snapshot TMPDIR per-run; giữ API positional/mode mặc định. Tách metadata encoder fixture M03 dùng lại, mở rộng validator đến M04, tích hợp Harness CI và audit documentation. Kết quả nghiệm thu thật tại handoff; chưa tạo tag M04 hoặc bắt đầu M05.

- Bốn workflows SUCCESS trên commit code M04 72df890; đã đọc artifact m04-reports, đối chiếu gate436/440, 15 host/69 guest tests, expected PANIC, protected hashes và cleanup. M04 vẫn VERIFYING, chưa merge/tag.

### Đóng M04 — 2026-10-27

- Người dùng xác nhận hoàn thành và duyệt tag/merge/bàn giao. Annotated milestone/M04 (object 90778d4b630c9fe5292f5d43c49cd6a91b8670b7) trỏ commit nghiệm thu 1c2f001896327d06a3875fa5b7443f847ae2ad7f, bốn workflows SUCCESS; đã push.
- PR #2 merge tại 78b5a60e54c5cdfb32f1ca32a4d4a42ece7fa78d; đồng bộ DONE và handoff. Không đổi tags M00–M03, không tạo Release hoặc bắt đầu M05.

### Triển khai M05 theo kế hoạch được duyệt — 2026-10-28

- Thêm Stage2 bootstrap32 KiB/core C32 có CRC/bounded E820/A20/thunk, GPT primary/backup validation, FAT32 slot A chỉ đọc và ELF64 higher-half loading; giữ Stage1440/ABI và LBP/GPT M03.
- Thêm parser API native, helper division unsigned64, loader first-fit/reservations/page tables và LBP builder; kernel probe kiểm state CPU/mappings/BSS và faults RO/guard thật. Chưa có kernel sản phẩm hoặc UEFI loader.
- Suite host/guest/sanitizer và fixtures sparse8 GiB test-only; harness purpose bios-stage2, test CPU/RAM/fault adapters; bổ sung CI M05 trong bốn workflows hiện có và validator trạng thái M05. Kết quả thật ở handoff; chưa tag/merge M05 hoặc triển khai M06.

- Bốn workflows code68cbd48 SUCCESS; artifact host/target đã đối chiếu: host45, guest51 mỗi profile, tổng102 guests đúng expected (14 PASS/88 PANIC), hashes binary/source khớp local và protected hashes/cleanup đúng. PR #3 open, M05 giữ VERIFYING chờ review/người dùng xác nhận riêng; không tag/merge hoặc bắt đầu M06.

- Review sau CI đầu sửa bootstrap để CRC/BSS/E820 dùng segment:offset real-mode chuẩn, loại phụ thuộc hidden segment limits. Thêm adapter64 KiB và core fixture70000 byte/corruption cuối file; giữ Stage1/LBP/GPT/budget. CI68cbd48 không thay nghiệm thu HEAD sửa.

- Revision sửa0a0f496 đã có bốn workflows SUCCESS; artifact mới xác nhận host46/guest54 mỗi profile,108 guest results đúng expected (18 PASS/90 PANIC), source/binary hashes khớp local và cleanup/protected hashes đúng. M05 vẫn VERIFYING, PR #3 chờ review/xác nhận riêng.

### Đóng M05 — 2026-10-28

- Người dùng xác nhận hoàn thành, duyệt tag/merge/bàn giao. Annotated milestone/M05 object d42c14b41a52f25f0fa45fa24ccd253069d8eae5 trỏ SHA nghiệm thu4742e2d5a3eceb51193e470ed3d7fcdb0495c401; bốn workflows SUCCESS, đã push.
- PR #3 đã merge tại c05b0b2435fef8bb5918befd99e5fcf2113365a1. Đồng bộ DONE/handoff, giữ tags M00–M04; không tạo version tag/Release hoặc bắt đầu M06.
