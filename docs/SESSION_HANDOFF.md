# Bàn giao M12 — VERIFYING — 2026-10-30

Trạng thái milestone: M00=DONE; M01=DONE; M02=DONE; M03=DONE; M04=DONE; M05=DONE; M06=DONE; M07=DONE; M08=DONE; M09=DONE; M10=DONE; M11=DONE; M12=VERIFYING

## M11 baseline đã nghiệm thu

- M11 acceptance commit `3bf87334999a794bce6dec7e8d691f5f125ff781`; annotated tag object `618d9a27610e5ce73c81abad120b04872f60587b` trỏ SHA đó. PR #9 merge commit `c1d288e7ca4fcc9f87c9eb08ba257098e6111bc8`.
- Bốn GitHub Actions workflows SUCCESS trên acceptance SHA: [Toolchain](https://github.com/duclam2011dz/LamixOS/actions/runs/41048803138), [Harness](https://github.com/duclam2011dz/LamixOS/actions/runs/41048803129), [Documentation](https://github.com/duclam2011dz/LamixOS/actions/runs/41048803013), [Boot Contract](https://github.com/duclam2011dz/LamixOS/actions/runs/41048803313).
- M11 CTest 3/3 PASS; regression M07 2/2, M08 2/2, M09 3/3, M10 3/3 PASS; docs validator PASS, unittest 39/39 PASS. PC thật và firmware ngoài profile QEMU NOT RUN.
- Kernel heap M11 là BSP free-list first-fit, dùng PMM/VMM qua callbacks; không reentrant/IRQ-safe, không có global C++ `new/delete`.

## M12 implementation và ranh giới

- Nhánh `codex/m12-buddy-pmm`, bắt đầu từ workspace M11 handoff. Thay đổi gồm `kernel/pmm.c`/`pmm.h`, kernel M12 profile, build/image tooling, `tests/m12/`, Toolchain CI và tài liệu M12. Chưa commit, push, mở PR hoặc tạo tag.
- Buddy dùng tree-state 2-bit/node cho miền dưới 4 GiB; order 0–18. Metadata dự trữ 512 KiB node state + 64 KiB boundary array = 589,824 bytes. Chỉ `LBP_USABLE` đầy đủ được cấp; reservation/hole/extent boundaries chặn merge. Không reclaim LOADER/ACPI, không dùng >4 GiB.
- Bitmap giữ mặc định và đường rollback; chỉ profile M12 route VMM/heap qua Buddy order 0. LBP v1, entry ABI, loaders, BIOS Stage1 gate và Stage2 budget không đổi.

## Kết quả local

- Cross build profile `m12-kernel` PASS. Kernel ELF SHA-256 theo build hiện tại: xem `build/m12/build.json` (artifact không version-control).
- M12 CTest 6/6 PASS: Buddy host ASan/UBSan, host provenance, benchmark và guest BIOS/UEFI cùng kernel. Cả hai guest phát M08–M12 PASS; protected hashes không đổi và cleanup không cưỡng bức.
- M11 CTest regression 3/3 PASS. M07 2/2, M08 2/2, M10 3/3 PASS. M09 sanitizer PASS; host/guest setup bị sandbox chặn khi build wrapper ghi dưới `build/`; artifacts cũ không tính là evidence hiện hành. Docs validator PASS, unittest 40/40 PASS; `git diff --check` PASS.
- Host benchmark: seed `0x12a110c`, trace hash `3ded994161041ff7`, 20,000 operations × 5 (100,000 per backend). Metadata bitmap 262,144 bytes/Buddy 589,824 bytes; mixed-order host trace orders 0–3, 100,000 phép đo: bitmap p50/p95/max 21/383/13,016 ns, Buddy 56/963/38,456 ns; mean init 810,773/7,920,754 ns; throughput 9,152,882/4,710,185 ops/s; latency maxima vary across host runs and are descriptive only. Alternating-free fragmentation fixture: cả hai còn 2,048 frames, largest 4 KiB, order1 allocation fail. Đây là host-only evidence, không phải guest performance; bitmap vẫn mặc định.
- M12 GitHub CI NOT RUN (chưa push/PR); PC thật và firmware ngoài profile QEMU NOT RUN. Chưa nghiệm thu DONE, chưa tạo milestone tag.

## Hành động tiếp theo

Rà soát diff, chạy docs validator/unittest, regressions cần thiết và kiểm tra report cuối. M12 vẫn VERIFYING tới khi có review/CI phù hợp và người dùng xác nhận nghiệm thu; không bắt đầu M13.
