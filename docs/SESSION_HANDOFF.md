# Bàn giao M10 — VERIFYING — 2026-10-29

Trạng thái milestone: M00=DONE; M01=DONE; M02=DONE; M03=DONE; M04=DONE; M05=DONE; M06=DONE; M07=DONE; M08=DONE; M09=DONE; M10=VERIFYING

## Trạng thái và Git

M10 đã được triển khai và kiểm chứng local theo kế hoạch. Checkout bắt đầu trên main tại SHA M09 closure 481efbcb3fa6e4144e9401e06f8745cee64de2f5; các thay đổi M10 hiện chưa commit. Không tạo commit, tag, PR hay merge. GitHub CI trên SHA chứa M10 NOT RUN vì chưa có commit/PR; trạng thái branch/remote cần xác minh lại trước thao tác Git tiếp theo.

## Thay đổi

- Thêm VMM kernel BSP paging 4 cấp với root CR3 mới; map/unmap/protect trang 4 KiB và range, query, TLB invalidation, rollback range, thu hồi bảng trung gian rỗng và enforcement W^X/NX theo LBP feature.
- Dựng lại kernel segments, BootInfo identity RO, stack identity RW, guard không map và framebuffer identity uncached. Page-table frames do PMM cấp và truy cập qua scratch data/control aliases; không tạo direct map. Leaf frames vẫn do caller sở hữu; TABLES cũ tiếp tục unavailable.
- Thêm M10 build profile/provenance, host simulator/sanitizer, image checker support cho m10-kernel và BIOS/OVMF guest fault profiles. M05 default cùng M08/M09 profiles được giữ nguyên.
- Sửa allocator scratch cũ để không tái sử dụng table page rỗng nhưng còn được CR3 hiện tại tham chiếu. Không đổi LBP v1, kernel entry ABI, loader, BIOS Stage1 gate hoặc Stage2 budget.

## Kiểm chứng local

- M10 CTest 3/3 PASS: VMM native ASan/UBSan, host build/image checks và BIOS/OVMF guest suite. Guest 8/8 PASS trên cùng kernel profiles: normal, ghi RO, execute NX và guard #PF trên cả hai firmware. Normal phát M08:PASS, M09:PASS, M10:PASS; fault profiles xác nhận vector 0e, error bits và CR2 mong đợi.
- Normal M10 kernel ELF SHA-256 5a0d8ec3a9131b0de5fffe02ef23144ea59cace380f67d58d78db82094634ef6 khớp BIOS và UEFI reports.
- Regression M08 CTest 2/2 PASS; M09 CTest 3/3 PASS; M07 CTest 2/2 PASS. Hash image extents, firmware và kernel trước/sau guest không đổi; QEMU cleanup không bị cưỡng bức.
- Docs validator PASS; unittest tài liệu 38/38 PASS; diff/whitespace checks PASS cho tracked và file mới.
- PC thật, CPU/firmware ngoài QEMU profiles, SMP, RAM trên 4 GiB, process address spaces và kernel sản phẩm NOT RUN/ngoài phạm vi. M10 chưa được xác nhận DONE.

## Lệnh M10

    python tests/m10/build_kernel.py --build build/m10
    PATH="$PWD/.venv/bin:$PATH" .venv/bin/cmake -S tests/m10 -B build/m10-tests -G Ninja -DPython3_EXECUTABLE="$PWD/.venv/bin/python"
    PATH="$PWD/.venv/bin:$PATH" .venv/bin/cmake --build build/m10-tests
    PATH="$PWD/.venv/bin:$PATH" .venv/bin/ctest --test-dir build/m10-tests --no-tests=error --output-on-failure

Bước tiếp theo: hoàn tất validator/unittest và diff review, sau đó để người dùng quyết định bước xác minh CI/PR và nghiệm thu milestone. Không bắt đầu M11.
