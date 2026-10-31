# Lịch sử thay đổi

Theo dạng Keep a Changelog đơn giản. Chỉ ghi thay đổi thực tế; chưa có release.

## Unreleased

### M17 — DONE (2026-10-31)

- Thêm VirtIO PCI split queue và VirtIO-GPU 2D scanout/flush, giữ software renderer M16 làm nguồn pixel; thêm M17 build profile, host sanitizer/provenance tests, guest BIOS/UEFI và framebuffer fallback.
- M17 CTest **3/3 PASS**, guest BIOS/UEFI GPU và fallback **4/4 PASS**, screenshots BIOS/UEFI đã kiểm tra trực quan. M15/M16 host hồi quy, M07 host, QMP fixtures, docs validator và unittest **41/41 PASS**. Hai review finding P1 được sửa và resolve: liên kết artifact local được thay bằng path dạng code; request/response DMA dùng storage persistent qua reset, có late-response sanitizer regression.
- PR [#15](https://github.com/duclam2011dz/LamixOS/pull/15) merge commit `8afa222fc493c2adbac71bb415adacc03b4510f7`; acceptance commit `292eb59f3c368c2933a7dbb0d135b89f4fb74415`; annotated tag `milestone/M17` object `83a72d02ee17ecc0608438a0ea4a18ec1d7a9262` trỏ acceptance commit. Bốn workflow SUCCESS trên đúng SHA: Toolchain [41200982371](https://github.com/duclam2011dz/LamixOS/actions/runs/41200982371), Harness [41200982402](https://github.com/duclam2011dz/LamixOS/actions/runs/41200982402), Documentation [41200982454](https://github.com/duclam2011dz/LamixOS/actions/runs/41200982454), Boot Contract [41200982320](https://github.com/duclam2011dz/LamixOS/actions/runs/41200982320). PC thật/firmware khác **NOT RUN**.
- Bootable raw GPT image dùng chung BIOS/UEFI và screenshot/report/logs được giữ cục bộ trong `out/m17/`; không commit image hoặc firmware.

### M16 — DONE (2026-10-31)

- Thêm software framebuffer renderer với shadow XRGB8888, primitives, clipping, ASCII bitmap font, dirty-region present và checksum; thêm BIOS VBE handoff chỉ trong profile M16, giữ LBP v1.
- M16 CTest 3/3 PASS; guest SeaBIOS/OVMF mỗi profile 1/1 PASS; hồi quy M07/Stage2 host/M15 host 54 PASS, docs validator PASS, unittest 41/41 PASS. PR #14 merge; bốn workflow CI PASS trên acceptance SHA `09c4248`. Annotated tag `milestone/M16` trỏ acceptance commit. PC thật/firmware ngoài profile QEMU NOT RUN.

### M15 — DONE (2026-10-31)

- Thêm kernel PS/2 keyboard/mouse, Set 1 decoder, US QWERTY ASCII và queue nội bộ SPSC 128 event; tích hợp IRQ1/IRQ12 vào APIC profile mới. QMP harness hỗ trợ kịch bản input sau marker.
- Kernel cross-build, host ASan/UBSan, guest BIOS/UEFI 3/3 mỗi profile, hồi quy M07–M14 và docs validator hiện PASS local. Bốn workflow GitHub Actions SUCCESS trên acceptance SHA `831748b5d65f0a88394745b20da82a56f69439c4`; M15 CTest 3/3, guest BIOS/UEFI 3/3 mỗi profile và hồi quy M07–M14 PASS. PR #13 merge commit `eba0607607545638fe1569fa129abac1b283d7d1`; annotated tag `milestone/M15` object `69aa3ebdb5c2843487b73925c8b73f96893cd2d7` trỏ acceptance SHA. PC thật/firmware ngoài QEMU NOT RUN.

### M14 — DONE (2026-10-31)

- Thêm RSDP validation/handoff vào BIOS Stage2 và UEFI bằng LBP v1 hiện có; BIOS Stage2 build nằm trong giới hạn 256 KiB, bootstrap 32 KiB.
- Mở rộng MADT parser, helper APIC/IOAPIC/timer, kernel profile M14 yêu cầu ACPI và host ASan/UBSan fixtures. Cross-build kernel, BIOS Stage2 build-only, UEFI build-only và host sanitizer hiện PASS.
- Guest SeaBIOS `pc` và OVMF `q35` cùng kernel/image, 3/3 lần mỗi firmware, đủ marker M14; M14 CTest 3/3 và hồi quy M05–M13 PASS. Bốn GitHub workflows SUCCESS trên acceptance SHA `f41d20df47b7e95ac9ac73027c7cd7a7395b67c5`; PR #12 merge commit `8360690506b1472d3b31959f36bb3a511e76282e`; annotated tag `milestone/M14` object `85876228135c5f1d971a1a30687e999ce5ddab39` trỏ acceptance SHA. PC thật/firmware ngoài profile NOT RUN. Xem [PLANS](PLANS.md) và [handoff](docs/SESSION_HANDOFF.md).

### M13 — DONE (2026-10-31)

- Thêm ACPI table walker RSDT/XSDT, checksum/bounds validation, MADT entry parser và MCFG parser; không đổi LBP v1 hoặc thu hồi ACPI memory.
- Thêm PCI segment 0 legacy config mechanism #1, ECAM theo MCFG, enumeration, BAR sizing/restoration, uncached MMIO mapping API và device registry với DMA ownership token; chưa cấp DMA buffer hoặc bật bus mastering.
- Host ASan/UBSan, kernel cross build và guest BIOS/UEFI PCI enumeration/MMIO mapping local PASS; M13 CTest 3/3, regression M07–M12 PASS, docs unittest 40/40 PASS. Bốn workflow GitHub Actions SUCCESS trên acceptance SHA `12d3ef1ac9040f9d523bb05a82066dadcaec6ecf`.
- PR #11 merge commit `26f3b3698f803fbcc8c8a39e7c9b4f01a631a7e5`; annotated tag `milestone/M13` object `ff2b1fbbae53d37af78aed566f381ac1b122f2d6` trỏ acceptance commit. Cả hai guest báo ACPI absent nên guest traversal **NOT RUN**; PC thật/firmware khác **NOT RUN**. Xem [PLANS](PLANS.md) và [handoff](docs/SESSION_HANDOFF.md).

### M12 — DONE (2026-10-30)

- Thêm Buddy PMM thử nghiệm song song với bitmap; bitmap vẫn mặc định. API cấp frame order 0 và cấp block order 0–18; kernel profile M12 route VMM/heap qua Buddy.
- Thêm native ASan/UBSan, provenance/guest BIOS-UEFI tests và benchmark host tái lập. M12 CTest **6/6 PASS**, M11 hồi quy **3/3 PASS**; BIOS/UEFI cùng kernel phát marker M08–M12.
- Benchmark host bỏ no-op khỏi mẫu latency/throughput và báo số no-op riêng. Lần chạy kiểm tra sau hiệu chỉnh ghi nhận throughput bitmap cao hơn Buddy; fragmentation fixture cho hai backend kết quả giống nhau. Đây là host-only measurement, không khẳng định lợi thế hiệu năng; metadata reserve Buddy **589,824 bytes**, bitmap **262,144 bytes**.
- PR #10 merged tại `385f835b5825528915080859156e963db15e6fd0`; annotated tag `milestone/M12` object `b4a0ee9ed47e3e6b77a8658e5270d8af8338bffe` trỏ acceptance commit `f3391d66eabc6b6d82a27800db6e229bb74f577b`. Bốn workflow CI SUCCESS trên SHA nghiệm thu; M12 CTest 6/6 PASS, guest BIOS/UEFI cùng kernel PASS. Review finding benchmark đã được sửa và resolve.
- Benchmark CI chỉ tính allocator calls, no-op báo riêng; kết quả host run trong [PLANS.md](PLANS.md). Bitmap tiếp tục mặc định vì Buddy có throughput thấp hơn và không cải thiện fragmentation fixture đã đo. PC thật và firmware ngoài profile QEMU **NOT RUN**. Không đổi boot contract, loader, BIOS Stage1 gate, Stage2 budget, ABI hoặc bắt đầu M13.


### M11 — DONE (2026-10-30)

- Thêm kernel heap BSP free-list first-fit dùng PMM/VMM, page backing theo nhu cầu, split/coalesce, C API alloc/calloc/free/stats và rollback khi mở rộng thất bại. Không thêm `new/delete`, slab, SMP hoặc IRQ-safe allocation.
- Thêm C++17 freestanding consumer/build provenance, host ASan/UBSan, image profile `m11-kernel`, BIOS/OVMF guest test và Toolchain CI job/report.
- Local M11 CTest **3/3 PASS**; hồi quy M07 **2/2**, M08 **2/2**, M09 **3/3**, M10 **3/3 PASS**. Bốn workflow SUCCESS trên SHA nghiệm thu `3bf87334999a794bce6dec7e8d691f5f125ff781`; PR #9 merge commit `c1d288e7`. Annotated tag `milestone/M11` trỏ commit nghiệm thu.

### M10 — DONE (2026-10-30)

- Thêm VMM BSP paging bốn cấp với root CR3 mới, 4 KiB map/unmap/protect/range, scratch access cho page-table frames, TLB invalidation, W^X/NX và cleanup bảng rỗng.
- Thêm kernel M10 provenance, host/sanitizer/BIOS/OVMF tests và M07 image checker support cho profile `m10-kernel`; kernel mặc định M05 và M08/M09 profiles giữ nguyên.
- Review sửa query quyền execute khi NX không khả dụng và thêm assertion native; M10 CTest sau sửa **3/3 PASS**, guest **8/8 PASS**. Docs validator/unittest **38/38 PASS**; hồi quy M07 **2/2**, M08 **2/2**, M09 **3/3 PASS**.
- PR #8 merge commit `493cd8a8`; bốn workflow GitHub Actions SUCCESS trên head `7e0b2f288e0497deb4a2c5fd4a50c679643643ba`. Annotated tag `milestone/M10` giữ nguyên trên commit triển khai đầu `dd6365c6`; review-fix commit nằm sau tag trong merge history.

### M09 — DONE (2026-10-29)

- Thêm bitmap PMM nội bộ 2-bit/frame trong kernel `.bss`; chỉ cấp frame 4 KiB thuộc `LBP_USABLE` dưới 4 GiB, loại reservations và không reclaim LOADER/ACPI.
- Thêm M09 kernel build provenance, host tests, ASan/UBSan stress test và QEMU guest test thật trên cùng kernel qua SeaBIOS/OVMF; mở rộng image checker nhận `m09-kernel`, giữ mặc định M05 và profile M08.
- Local: M09 CTest **3/3 PASS**, host pytest **272 passed/139 deselected**, docs validator và unittest **37/37 PASS**; hồi quy M02–M08 như [handoff](docs/SESSION_HANDOFF.md). Bốn workflow GitHub Actions SUCCESS trên SHA `b2ef8182`.
- PR #7 merge commit `49720af5`; annotated tag `milestone/M09` tag object `bcb23b86`, peeled commit nghiệm thu `b2ef8182`.

### M08 — DONE (2026-10-29)

- Thêm kernel freestanding C/NASM, ELF build provenance, LBP v1 consumer, GDT/IDT/TSS+double-fault IST, COM1 panic context và PIC/PIT IRQ0 self-test.
- M07 image generator nhận tùy chọn `--kernel-build`; mặc định tiếp tục dùng kernel M05. Thêm BIOS/OVMF guest tests cho normal, `#UD`, guard `#PF` và đưa M08 vào Toolchain CI.
- Bổ sung scratch mapping/span cho ACPI RSDP trước `lbp_validate()`; gỡ mapping sau validation. OVMF hiện không cung cấp RSDP căn 16 byte nên guest path ACPI hợp lệ ghi NOT RUN.
- M08 local: host 3/3, QEMU guest 6/6; host pytest 266 passed/137 deselected, docs validator và unittest 36/36 PASS. Bốn workflow CI SUCCESS trên SHA `56cbc3bd`.
- PR #6 merge commit `6b794a6c`; annotated tag `milestone/M08` trỏ commit nghiệm thu `56cbc3bd` (tag object `ba5642cf`).

### M07 — DONE (2026-10-29)

- Thêm Python generator/checker GPT image raw sparse 8 GiB; populate FAT32 ESP bằng `dosfstools`/`mtools`, giữ System A/B và User Data chưa format. GUID random mặc định, seed UUIDv5 tùy chọn.
- Thêm host checks và QEMU parity tests; cùng image/kernel probe boot qua SeaBIOS và OVMF, so sánh LBP v1 contract, xác minh protected image/firmware và negative ELF.
- Thêm dòng `M07:LBP` trong kernel probe, doctor dependency checks và Toolchain CI M07 guest job. Không đổi LBP wire layout, BIOS Stage1 gate hoặc Stage2 budget.
- Local M07 CTest 2/2 sau race-fix; regression và host/docs suites đạt theo handoff. PR #5 merge commit `93724991`; tag annotated `milestone/M07` trỏ commit nghiệm thu `1ece4235`. Bốn workflow CI SUCCESS trên SHA nghiệm thu.


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

### Triển khai M06 theo kế hoạch được duyệt — 2026-10-28

- Thêm EFI application PE/COFF x64 bằng Clang+LLD, runtime freestanding, UEFI ABI declarations và trampoline sang kernel ABI System V. Dùng chung GPT/ELF/LBP/memory code; xác định ESP từ Device Path/Block I/O/GPT; đọc slot A và không fallback B.
- Thêm GOP framebuffer identity mapping và ACPI RSDP optional record, UEFI memory map decode/normalize, bounded map-buffer growth và retry `ExitBootServices()`. LBP wire layout không đổi; validator/probe kiểm mapping framebuffer kể cả physical trên4 GiB.
- Mở rộng fixture/harness opt-in cho OVMF, PE inspection, guest fault cases và reports; Toolchain CI cài clang/lld và chạy M06 OVMF guest. M07 generator/parity không triển khai.
- Local: M06 CTest host retry/capacity policy + PE + 5 guest cases PASS; M05 BIOS Debug/Release 4/4 mỗi profile, M05 sanitizer1/1; M03 Debug/Release4/4 và sanitizer3/3; M02 launch/fixture2 PASS, skeleton2 SKIPPED; M04 CTest2/2. OVMF positive xác nhận GOP; ACPI bị bỏ do RSDP không căn16; PC thật/4Kn NOT RUN.

### Đóng M06 — 2026-10-29

- PR #4 đã merge tại `74db2721c88eaccd22fd85dc26b1ddbd29bf4bef`. Annotated tag `milestone/M06` object `c5e396f5d2e7a3f8d4d3ccdcbf2eb3c3ea75d9f3` trỏ commit nghiệm thu `b96c6ba3b0dbb5c60583e40bc83406e1b7eac10f`.
- Documentation, Boot Contract, Harness và Toolchain CI đều SUCCESS trên commit nghiệm thu. Review findings về LBP count, GOP masks và build target CTest đã được sửa; local M06 CTest 3/3 PASS.
- M06 DONE; M07 vẫn PLANNED và chưa bắt đầu. Không tạo version tag hoặc GitHub Release.
