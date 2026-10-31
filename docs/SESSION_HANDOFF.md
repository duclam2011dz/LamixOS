# Bàn giao M16 — IN_PROGRESS — 2026-10-31

Trạng thái milestone: M00=DONE; M01=DONE; M02=DONE; M03=DONE; M04=DONE; M05=DONE; M06=DONE; M07=DONE; M08=DONE; M09=DONE; M10=DONE; M11=DONE; M12=DONE; M13=DONE; M14=DONE; M15=DONE
Trạng thái M16: IN_PROGRESS; M17–M44: PLANNED

## Tiếp tục M16

- Renderer XRGB8888, backbuffer kernel heap, primitives/text ASCII, damage/present/checksum và profile BIOS VBE đã được thêm. Chi tiết thiết kế và lệnh kiểm thử ở [PLANS](../PLANS.md), [GRAPHICS](GRAPHICS.md), [TESTING](TESTING.md).
- M16 CTest local **3/3 PASS**: native ASan/UBSan, host provenance và guest BIOS/UEFI; SeaBIOS `pc` **1/1 PASS**, OVMF `q35` **1/1 PASS**. BIOS Stage2 Release core file **41,768 byte**, core/BSS **176,128/262,144 byte**, bootstrap **32 KiB**; kernel cross-build PASS.
- Regression M07 + Stage2 host + M15 host **54 PASS, 3 QEMU cases deselected**; M16 guest suite tự kiểm BIOS/UEFI thật. Docs validator **PASS**, unittest tài liệu **41/41 PASS**. GitHub CI **NOT RUN**. PC thật/firmware ngoài profile QEMU **NOT RUN**. M16 chưa DONE; chưa tạo tag/PR/merge. Chưa bắt đầu M17.

## M15 đã nghiệm thu

- PR #13 merge commit `eba0607607545638fe1569fa129abac1b283d7d1`. Acceptance commit `831748b5d65f0a88394745b20da82a56f69439c4`; annotated tag `milestone/M15` object `69aa3ebdb5c2843487b73925c8b73f96893cd2d7` trỏ acceptance SHA.
- Bốn workflow SUCCESS trên SHA nghiệm thu: Toolchain [41180666554](https://github.com/duclam2011dz/LamixOS/actions/runs/41180666554), Harness [41180666561](https://github.com/duclam2011dz/LamixOS/actions/runs/41180666561), Documentation [41180666589](https://github.com/duclam2011dz/LamixOS/actions/runs/41180666589), Boot Contract [41180666560](https://github.com/duclam2011dz/LamixOS/actions/runs/41180666560). Review finding 9-bit mouse delta được sửa, test ±200 thêm, thread resolved.
- M15 CTest 3/3 PASS; guest SeaBIOS `pc` và OVMF `q35` cùng kernel/image 3/3 boots mỗi profile; hồi quy M07–M14 PASS, QMP harness 35/35 PASS, docs validator/unittest 41/41 PASS. Reports tại `out/m15/results/`.
- PC thật và firmware ngoài profile QEMU **NOT RUN**. M16 chưa bắt đầu; cần Plan Mode và duyệt riêng.

## M14 đã nghiệm thu

- PR [#12](https://github.com/duclam2011dz/LamixOS/pull/12) merge vào `main` tại `8360690506b1472d3b31959f36bb3a511e76282e`. Acceptance commit `f41d20df47b7e95ac9ac73027c7cd7a7395b67c5`; annotated tag `milestone/M14` object `85876228135c5f1d971a1a30687e999ce5ddab39` trỏ acceptance commit.
- Bốn workflow SUCCESS trên acceptance SHA: [Toolchain CI](https://github.com/duclam2011dz/LamixOS/actions/runs/41177278867), [Harness CI](https://github.com/duclam2011dz/LamixOS/actions/runs/41177278863), [Documentation CI](https://github.com/duclam2011dz/LamixOS/actions/runs/41177278850), [Boot Contract CI](https://github.com/duclam2011dz/LamixOS/actions/runs/41177278957). Review tự động trên `f41d20d` không có major findings; thread hiệu chuẩn LAPIC đã sửa và resolved.
- M14 dùng LBP v1 để handoff RSDP, khởi tạo BSP xAPIC/IOAPIC, route PIT IRQ0 làm tham chiếu và chuyển sang LAPIC timer. Không đổi wire layout, entry ABI, BIOS Stage1 gate hoặc Stage2 budget.
- Kernel cross-build, BIOS Stage2 build-only (core 172,032/262,144 byte; bootstrap 32 KiB), UEFI build-only và native ASan/UBSan M14 **PASS**.
- Guest cùng kernel/image: SeaBIOS `pc` **3/3 PASS**, OVMF `q35` **3/3 PASS**; đủ marker RSDP/MADT, xAPIC, IOAPIC, PIT reference, LAPIC timer và M14; protected hashes không đổi, cleanup sạch. Reports: `out/m14/results/bios.json`, `out/m14/results/uefi.json`.
- Toolchain CI M14 CTest **3/3 PASS**, M05 regression **4/4 PASS**, M06 UEFI policy/PE/guest **3/3 PASS**, hồi quy M07 2/2, M08 2/2, M09 3/3, M10 3/3, M11 3/3, M12 6/6, M13 3/3 **PASS**. M06 ACPI-required fixture xác nhận `required_features`/`provided_features` có ACPI. Docs validator trong CI **PASS**; unittest tài liệu local trước closure **41/41 PASS**. M06 CTest local sau sửa **NOT RUN** vì workspace closure không có `cmake`/`ctest`.
- PC thật và firmware ngoài profile QEMU **NOT RUN**. M15 đã đóng; M16 chưa bắt đầu.

## M13 đã nghiệm thu

- PR [#11](https://github.com/duclam2011dz/LamixOS/pull/11) merged vào `main` tại `26f3b3698f803fbcc8c8a39e7c9b4f01a631a7e5`. Acceptance commit `12d3ef1ac9040f9d523bb05a82066dadcaec6ecf`; annotated tag `milestone/M13` object `ff2b1fbbae53d37af78aed566f381ac1b122f2d6` trỏ acceptance commit. Commit được CI kiểm tra trước merge.
- Bốn workflow SUCCESS trên acceptance SHA: [Toolchain CI](https://github.com/duclam2011dz/LamixOS/actions/runs/41170371900), [Harness CI](https://github.com/duclam2011dz/LamixOS/actions/runs/41170371899), [Documentation CI](https://github.com/duclam2011dz/LamixOS/actions/runs/41170371842), [Boot Contract CI](https://github.com/duclam2011dz/LamixOS/actions/runs/41170371849). Automated review phát hiện ECAM offset nonzero-start và ACPI RSDT fallback; cả hai đã sửa trong acceptance commit, regression tests thêm, review threads resolved.
- Kernel M13 cross-build **PASS**; CTest **3/3 PASS** local. Guest SeaBIOS `pc` và OVMF `q35` cùng kernel boot thật, mỗi guest báo 5 PCI functions/devices, BAR MMIO mapping PASS; protected hashes giữ nguyên và cleanup sạch. Hồi quy local M07 2/2, M08 2/2, M09 3/3, M10 3/3, M11 3/3, M12 6/6 **PASS**; docs validator và unittest **40/40 PASS**.
- ACPI trên cả hai firmware guest: `M13:ACPI:ABSENT`. Host ASan/UBSan fixtures kiểm RSDP/RSDT/XSDT/MADT/MCFG, malformed inputs, RSDT fallback và ECAM range. Guest ACPI traversal **NOT RUN**, không được tính là guest PASS. PC thật và firmware ngoài profile QEMU **NOT RUN**.
- M13 **DONE** theo acceptance Roadmap. Bitmap PMM tiếp tục cấp frame cho VMM/heap; Buddy chỉ self-test. Không DMA buffer/bus mastering, không đổi LBP/loader/ABI. Tại thời điểm đóng M13, M14 còn PLANNED.

## M12 đã nghiệm thu

- PR [#10](https://github.com/duclam2011dz/LamixOS/pull/10) merged tại `385f835b5825528915080859156e963db15e6fd0` vào `main`.
- Acceptance commit `f3391d66eabc6b6d82a27800db6e229bb74f577b`. Annotated tag `milestone/M12` object `b4a0ee9ed47e3e6b77a8658e5270d8af8338bffe` trỏ acceptance commit. Commit closure tài liệu nằm sau tag; không di chuyển tag.
- Bốn GitHub Actions workflows SUCCESS trên acceptance SHA: [Toolchain CI](https://github.com/duclam2011dz/LamixOS/actions/runs/41084497139), [Harness CI](https://github.com/duclam2011dz/LamixOS/actions/runs/41084497177), [Documentation CI](https://github.com/duclam2011dz/LamixOS/actions/runs/41084497141), [Boot Contract CI](https://github.com/duclam2011dz/LamixOS/actions/runs/41084497179).
- Toolchain M12 CTest 6/6 PASS: ba Buddy ASan/UBSan cases, host provenance, benchmark và BIOS/UEFI guest. Cùng kernel SHA-256 `8ed07725bfa1fb5bb8d226c9d85cfe2f7efec82d45efe614a85998cc2c9ef847` boot qua SeaBIOS/OVMF; cả hai serial log có M08–M12 PASS, protected hashes giữ nguyên, cleanup không cưỡng bức.
- Hồi quy Toolchain: M07 2/2, M08 2/2, M09 3/3, M10 3/3, M11 3/3 PASS. Docs validator và unittest tài liệu 40/40 PASS local trước closure; benchmark native trực tiếp qua `cc` và count/schema validation PASS. CMake không có trong PATH local; không chạy local CTest sau review-fix. Toolchain CI đã chạy CTest trên SHA nghiệm thu.
- Review finding benchmark no-op đã được sửa trong `895e5ba`; thread review resolve trước merge. Lỗi CI M09 ở lượt đầu được sửa bằng cách chỉ biên dịch frame wrappers khi profile VMM bật.

## Kết quả và giới hạn Buddy

- Bitmap PMM tiếp tục là backend mặc định; Buddy là backend thử nghiệm song song trong profile `m12-kernel`. Buddy có order 0–18, quản lý frame đầy đủ thuộc `LBP_USABLE` dưới 4 GiB sau reservations. Không reclaim LOADER/ACPI, không cấp RAM trên 4 GiB.
- Benchmark CI seed `0x12a110c`, trace hash `3ded994161041ff7`, 20,000 trace operations × 5. Mỗi backend ghi 50,385 allocator calls và bỏ qua 49,615 no-op; latency p50/p95/max bitmap 203/580/11,902 ns, Buddy 180/1,425/11,440 ns; throughput 4,469,397/2,136,508 calls/s; mean init 1,124,849/10,944,121 ns. Metadata bitmap/Buddy 262,144/589,824 bytes.
- Fixture alternate-free: cả hai còn 2,048 frames, largest block 4 KiB; order 1 cấp phát thất bại dù tổng free đủ. Không thấy lợi thế fragmentation trong fixture này. Đây là số host CI, không phải hiệu năng guest; không chuyển Buddy thành mặc định.
- PC thật và firmware ngoài profile QEMU **NOT RUN**. Không thay LBP v1, kernel entry ABI, loaders, BIOS Stage1 gate hoặc Stage2 budget.

## M11 baseline

- M11 acceptance commit `3bf87334999a794bce6dec7e8d691f5f125ff781`; annotated tag object `618d9a27610e5ce73c81abad120b04872f60587b`; PR #9 merge commit `c1d288e7ca4fcc9f87c9eb08ba257098e6111bc8`.
- Kernel heap M11 là BSP free-list first-fit dùng PMM/VMM callbacks; không reentrant/IRQ-safe, không có global C++ `new/delete`.

## Tiếp theo tại thời điểm bàn giao M06 (ghi chú lịch sử)

M06 vừa đóng; khi đó M07 là milestone kế tiếp. Ghi chú lịch sử này không thay thế trạng thái hiện tại ở đầu handoff: M14 đã đóng; M15 đã đóng; M16 chưa bắt đầu.
