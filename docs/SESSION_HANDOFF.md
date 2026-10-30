# Bàn giao M11 — DONE — 2026-10-30

Trạng thái milestone: M00=DONE; M01=DONE; M02=DONE; M03=DONE; M04=DONE; M05=DONE; M06=DONE; M07=DONE; M08=DONE; M09=DONE; M10=DONE; M11=DONE

## Trạng thái Git và phát hành milestone

- PR [#9](https://github.com/duclam2011dz/LamixOS/pull/9) đã merge vào `main`, merge commit `c1d288e7ca4fcc9f87c9eb08ba257098e6111bc8`.
- Commit nghiệm thu M11: `3bf87334999a794bce6dec7e8d691f5f125ff781` (`feat(m11): add BSP kernel heap`). Annotated tag `milestone/M11` đã push; tag object `618d9a27610e5ce73c81abad120b04872f60587b`, peeled commit trỏ SHA nghiệm thu.
- Local `main` đã fast-forward tới merge commit. Commit handoff hiện tại sẽ nằm sau tag; tag nghiệm thu không di chuyển.
- Review tự động PR hoàn tất, không có findings. Không thay đổi lịch sử/tag M00–M10.

## M11 đã triển khai

- Thêm `kernel/heap.c`/`kernel/heap.h`: heap BSP-only free-list first-fit, chia/gộp block, căn 16 byte, cấp page theo nhu cầu trong aperture 1 GiB `0xffffe00000000000` qua PMM/VMM. Page mapped được giữ lại; lỗi mở rộng rollback page/frame mới.
- C API init/alloc/calloc/free/stats; calloc zero và kiểm tra overflow, alloc không zero. OOM/zero-size trả NULL; invalid pointer và double-free có status riêng. Heap không reentrant/IRQ-safe; chưa có global C++ `new/delete`.
- Thêm consumer C++17 freestanding, profile/provenance `m11-kernel`, image checker support, host sanitizer/guest tests và Toolchain CI job. Không đổi LBP v1, loader, BIOS Stage1 gate hoặc Stage2 budget.

## Bằng chứng nghiệm thu

- M11 CTest **3/3 PASS**: heap native ASan/UBSan, host provenance/C++ smoke và guest BIOS/OVMF. Cùng kernel ELF SHA-256 `9500f6c599c96a0430224cb86a9903412735802982308ed9a0c3fa9b7939b4a5` boot qua SeaBIOS/OVMF; serial có `M08:PASS`, `M09:PASS`, `M10:PASS`, `M11:PASS`.
- Hồi quy M07 **2/2**, M08 **2/2**, M09 **3/3**, M10 **3/3 PASS**. Docs validator **PASS**, unittest tài liệu **39/39 PASS**, `git diff --check` **PASS**.
- Bốn workflow SUCCESS trên SHA nghiệm thu `3bf8733`: [Toolchain CI](https://github.com/duclam2011dz/LamixOS/actions/runs/41048803138), [Harness CI](https://github.com/duclam2011dz/LamixOS/actions/runs/41048803129), [Documentation CI](https://github.com/duclam2011dz/LamixOS/actions/runs/41048803013), [Boot Contract CI](https://github.com/duclam2011dz/LamixOS/actions/runs/41048803313). Toolchain CI chạy M11 CTest/guest; reports không chứa image hoặc firmware.
- PC thật và firmware ngoài profile QEMU **NOT RUN**. M12 chưa bắt đầu; bước tiếp theo là yêu cầu Plan Mode riêng cho M12.
