# Bàn giao M10 — DONE — 2026-10-30

Trạng thái milestone: M00=DONE; M01=DONE; M02=DONE; M03=DONE; M04=DONE; M05=DONE; M06=DONE; M07=DONE; M08=DONE; M09=DONE; M10=DONE

## Kết quả và Git

M10 VMM đã triển khai, kiểm chứng, review và merge. PR [#8](https://github.com/duclam2011dz/LamixOS/pull/8) merge commit `493cd8a8e6790d0d1adbd87e7500b596698613dd`. Head nghiệm thu cuối cùng là `7e0b2f288e0497deb4a2c5fd4a50c679643643ba`; commit triển khai ban đầu `dd6365c6c217bfc076f73ec0cb5efdcb8e05a777`, commit sửa review `7e0b2f288e0497deb4a2c5fd4a50c679643643ba`.

Annotated tag `milestone/M10` đã push; tag object `667b9434a6713b8d596072557643648a7342fd24`, peeled commit `dd6365c6c217bfc076f73ec0cb5efdcb8e05a777`. Review phát hiện sau khi tạo tag một lỗi ở đường không có NX: `vmm_query()` báo mapping read-only là non-executable dù phần cứng thực thi được. Đã sửa và thêm native regression tại `7e0b2f2`, resolve thread review, chạy lại M10 tests/CI rồi merge. Tag công khai được giữ nguyên, không di chuyển; vì vậy nó trỏ commit triển khai đầu thay vì head cuối chứa review-fix.

Nhánh làm việc là `codex/m10-vmm-address-space`; local `main` hiện đã fast-forward tới PR merge. Bước tiếp theo là commit tài liệu bàn giao này và xác minh Documentation CI trên SHA đóng thủ tục; không bắt đầu M11.

## Thay đổi M10

- Thêm `kernel/vmm.c`/`kernel/vmm.h`: paging 4 cấp, root CR3 mới cho BSP, map/unmap/protect 4 KiB và range, query, rollback OOM/conflict, thu hồi bảng trung gian rỗng, scratch access, W^X, NX khi có, uncached mapping và `invlpg`.
- Dựng lại mapping kernel segments, BootInfo identity RO, stack identity RW, guard unmapped và framebuffer uncached. Không direct-map RAM; caller giữ ownership leaf frames; TABLES cũ vẫn unavailable.
- Thêm M10 build profile/provenance, native simulator/sanitizer và guest fault tests BIOS/OVMF; image tooling giữ M05 default và các profile M08/M09. Sửa allocator scratch cũ để không tái dùng table rỗng còn được CR3 hiện hành tham chiếu.
- Giữ nguyên LBP v1, kernel entry ABI, loaders, BIOS Stage1 gate/440-byte artifact và Stage2 budget.

## Kiểm chứng

- M10 CTest sau review-fix: **3/3 PASS**, guest **8/8 PASS** trên SeaBIOS/OVMF cho normal, RO write, NX execute và guard fault. Native ASan/UBSan và host/image tests PASS.
- Hồi quy M07 **2/2 PASS**, M08 **2/2 PASS**, M09 **3/3 PASS**. Docs validator PASS; unittest tài liệu **38/38 PASS**.
- Cả bốn GitHub Actions workflows **SUCCESS trên đúng PR head `7e0b2f288e0497deb4a2c5fd4a50c679643643ba`**: [Toolchain CI](https://github.com/duclam2011dz/LamixOS/actions/runs/40902948477), [Harness CI](https://github.com/duclam2011dz/LamixOS/actions/runs/40902948404), [Documentation CI](https://github.com/duclam2011dz/LamixOS/actions/runs/40902948422), [Boot Contract CI](https://github.com/duclam2011dz/LamixOS/actions/runs/40902948480).
- PC thật, firmware khác QEMU profile, SMP, RAM trên 4 GiB, process address spaces và kernel sản phẩm: **NOT RUN/ngoài phạm vi**. OS skeleton mặc định vẫn **SKIPPED**.

## Lệnh M10

    python tests/m10/build_kernel.py --build build/m10
    PATH="$PWD/.venv/bin:$PATH" .venv/bin/cmake -S tests/m10 -B build/m10-tests -G Ninja -DPython3_EXECUTABLE="$PWD/.venv/bin/python"
    PATH="$PWD/.venv/bin:$PATH" .venv/bin/cmake --build build/m10-tests
    PATH="$PWD/.venv/bin:$PATH" .venv/bin/ctest --test-dir build/m10-tests --no-tests=error --output-on-failure
