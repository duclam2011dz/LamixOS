# Bàn giao phiên M13 — IN_PROGRESS — 2026-10-30

Trạng thái milestone: M00=DONE; M01=DONE; M02=DONE; M03=DONE; M04=DONE; M05=DONE; M06=DONE; M07=DONE; M08=DONE; M09=DONE; M10=DONE; M11=DONE; M12=DONE; M13=IN_PROGRESS

## M13 hiện tại

- Workspace bắt đầu từ `main`/`67229fe747f59a53c4e199858b091513770a2887`, clean trước thay đổi. Tạo branch bị chặn vì `.git` read-only; chưa commit/push. `git ls-remote origin main` **NOT RUN** do DNS.
- Kernel M13 build PASS; CTest **3/3 PASS** local. Guest SeaBIOS `pc` và OVMF `q35` cùng kernel boot thật, mỗi guest báo 5 PCI functions/devices, BAR MMIO mapping PASS; protected hashes giữ nguyên và cleanup sạch.
- ACPI trên cả hai firmware guest: `M13:ACPI:ABSENT`. Host ASan/UBSan fixtures kiểm RSDP/XSDT/MADT/MCFG, malformed inputs; ACPI traversal guest **NOT RUN**. Hồi quy local M07 2/2, M08 2/2, M09 3/3, M10 3/3, M11 3/3, M12 6/6 **PASS**; GitHub CI **NOT RUN** do DNS.
- M13 vẫn **IN_PROGRESS**. Bitmap PMM vẫn cấp frame cho VMM/heap; Buddy chỉ self-test. Không DMA buffer/bus mastering, không đổi LBP/loader. Chưa bắt đầu M14.

## M12 đã nghiệm thu

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

## Tiếp theo

Tiếp tục M13 sau khi xử lý đường cấp ACPI guest được hỗ trợ và chạy regression M07–M12/GitHub CI. Chỉ chuyển M13 sang DONE khi guest ACPI và mọi tiêu chí Roadmap có bằng chứng. Không bắt đầu M14 trong lượt tiếp theo nếu chưa có kế hoạch/duyệt riêng.
