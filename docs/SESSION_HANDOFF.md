# Bàn giao M12 — VERIFYING — 2026-10-30

Trạng thái milestone: M00=DONE; M01=DONE; M02=DONE; M03=DONE; M04=DONE; M05=DONE; M06=DONE; M07=DONE; M08=DONE; M09=DONE; M10=DONE; M11=DONE; M12=VERIFYING

## M11 baseline đã nghiệm thu

- M11 acceptance commit `3bf87334999a794bce6dec7e8d691f5f125ff781`; annotated tag object `618d9a27610e5ce73c81abad120b04872f60587b` trỏ SHA đó. PR #9 merge commit `c1d288e7ca4fcc9f87c9eb08ba257098e6111bc8`.
- Bốn GitHub Actions workflows SUCCESS trên acceptance SHA: [Toolchain](https://github.com/duclam2011dz/LamixOS/actions/runs/41048803138), [Harness](https://github.com/duclam2011dz/LamixOS/actions/runs/41048803129), [Documentation](https://github.com/duclam2011dz/LamixOS/actions/runs/41048803013), [Boot Contract](https://github.com/duclam2011dz/LamixOS/actions/runs/41048803313).
- M11 CTest 3/3 PASS; regression M07 2/2, M08 2/2, M09 3/3, M10 3/3 PASS; docs validator PASS, unittest 39/39 PASS. PC thật và firmware ngoài profile QEMU NOT RUN.
- Kernel heap M11 là BSP free-list first-fit, dùng PMM/VMM qua callbacks; không reentrant/IRQ-safe, không có global C++ `new/delete`.

## M12 implementation và ranh giới

- Nhánh `codex/m12-buddy-pmm`, bắt đầu từ workspace M11 handoff. M12 implementation commit `214229507800f1c41ce34f8c55c3fd42662454c7`, PR #10 đang mở; tag chưa tạo. Thay đổi gồm `kernel/pmm.c`/`pmm.h`, kernel M12 profile, build/image tooling, `tests/m12/`, Toolchain CI và tài liệu M12.
- Buddy dùng tree-state 2-bit/node cho miền dưới 4 GiB; order 0–18. Metadata dự trữ 512 KiB node state + 64 KiB boundary array = 589,824 bytes. Chỉ `LBP_USABLE` đầy đủ được cấp; reservation/hole/extent boundaries chặn merge. Không reclaim LOADER/ACPI, không dùng >4 GiB.
- Bitmap giữ mặc định và đường rollback; chỉ profile M12 route VMM/heap qua Buddy order 0. LBP v1, entry ABI, loaders, BIOS Stage1 gate và Stage2 budget không đổi.

## Kết quả local

- Cross build profile `m12-kernel` PASS. Kernel ELF SHA-256 theo build hiện tại: xem `build/m12/build.json` (artifact không version-control).
- M12 CTest 6/6 PASS: Buddy host ASan/UBSan, host provenance, benchmark và guest BIOS/UEFI cùng kernel. Cả hai guest phát M08–M12 PASS; protected hashes không đổi và cleanup không cưỡng bức.
- M11 CTest regression 3/3 PASS. M07 2/2, M08 2/2, M10 3/3 PASS. M09 sanitizer PASS; host/guest setup bị sandbox chặn khi build wrapper ghi dưới `build/`; artifacts cũ không tính là evidence hiện hành. Docs validator PASS, unittest 40/40 PASS; `git diff --check` PASS.
- Host benchmark sau hiệu chỉnh: seed `0x12a110c`, trace hash `3ded994161041ff7`, 20,000 trace operations × 5; no-op bị bỏ khỏi latency/throughput và báo riêng. Lần chạy native ghi nhận 50,385 allocator calls và 49,615 no-op mỗi backend; p50/p95/max bitmap 130/385/23,723 ns, Buddy 128/1,015/53,576 ns; throughput 6,762,287/2,979,290 calls/s; mean init 768,026/7,574,075 ns. Các giá trị dao động và chỉ mô tả lần host run này. Metadata bitmap/Buddy 262,144/589,824 bytes. Alternating-free fixture vẫn bằng nhau: 2,048 frames rỗi, largest 4 KiB, order 1 allocation fail. Đây không phải guest performance evidence; bitmap vẫn mặc định.
- CI PR #10 trên SHA `214229507800f1c41ce34f8c55c3fd42662454c7`: Documentation [41082791435](https://github.com/duclam2011dz/LamixOS/actions/runs/41082791435), Harness [41082791590](https://github.com/duclam2011dz/LamixOS/actions/runs/41082791590) và Boot Contract [41082791478](https://github.com/duclam2011dz/LamixOS/actions/runs/41082791478) SUCCESS. Toolchain [41082791636](https://github.com/duclam2011dz/LamixOS/actions/runs/41082791636) FAILURE hai lượt ở smoke/CTest M01, trước bước M12; M12 CI step SKIPPED. Artifact chỉ ra wrapper frame PMM không dùng trong profile M09 dưới `-Werror`; workspace đã giới hạn wrapper cho profile VMM và syntax-only `cc` M09/M12 PASS, chờ CI xác nhận. PC thật và firmware ngoài profile QEMU NOT RUN. M12 chưa nghiệm thu DONE, chưa merge, chưa tạo milestone tag.
- Review tự động PR #10 ghi nhận benchmark tính no-op vào latency/throughput; đã sửa và đẩy commit `895e5ba74250b09ac1113bb5f9bc2b7a72e3da2a`. Benchmark native compile và schema/count validation qua direct `cc` **PASS** sau sửa; CMake/CTest local **NOT RUN** vì `cmake` không có trong PATH. Chưa xác minh review thread sau commit sửa.

## Hành động tiếp theo

Tiếp tục xử lý lỗi Toolchain CI và đẩy bản sửa benchmark sau review; cần toàn bộ CI xanh và review finding được resolve trước merge/tag. M12 vẫn VERIFYING; không bắt đầu M13.
