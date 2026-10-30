# Bàn giao M11 — VERIFYING — 2026-10-30

Trạng thái milestone: M00=DONE; M01=DONE; M02=DONE; M03=DONE; M04=DONE; M05=DONE; M06=DONE; M07=DONE; M08=DONE; M09=DONE; M10=DONE; M11=VERIFYING

M11 kernel heap đã triển khai trên nhánh `codex/m11-kernel-heap`, dựa trên local `main` SHA `bddbc148191cc10e66a6cb9506ec62e8b1258237`. Remote `origin` là repository chính thức `https://github.com/duclam2011dz/LamixOS.git`. Thay đổi chưa commit/push, chưa có PR hoặc tag M11. Bốn workflow GitHub Actions trên SHA chứa thay đổi chưa chạy; vì vậy M11 giữ **VERIFYING**, chưa DONE.

## Thay đổi

- Thêm `kernel/heap.c`/`kernel/heap.h`: allocator BSP-only free-list first-fit, chia/gộp block, căn 16 byte và page backing lười trong aperture 1 GiB `0xffffe00000000000`. Page đã map được giữ để tái sử dụng; lỗi mở rộng rollback pages/frame vừa cấp.
- Thêm API C init/alloc/calloc/free/stats; zeroing chỉ ở calloc. OOM trả NULL; zero-size trả NULL; overflow calloc, invalid pointer và double-free được xử lý bằng status. Heap không reentrant/IRQ-safe; chưa có global C++ `new/delete`.
- Kernel tự kiểm tra heap sau PMM/VMM, dùng consumer C++17 freestanding; build kiểm tra provenance, ELF không undefined runtime symbols và không có static constructors. Image tooling hỗ trợ `m11-kernel`, giữ mặc định M05 và profile cũ.
- Toolchain CI đã được cấu hình thêm M11 CTest cùng report artifacts; kết quả CI trên thay đổi hiện vẫn **PENDING/NOT RUN**.

## Bằng chứng local

- M11 CTest **3/3 PASS**: heap native ASan/UBSan, host provenance/C++ cross smoke và BIOS/OVMF guest suite. Cùng kernel ELF SHA-256 `9500f6c599c96a0430224cb86a9903412735802982308ed9a0c3fa9b7939b4a5` boot qua cả SeaBIOS và OVMF; serial chứa `M08:PASS`, `M09:PASS`, `M10:PASS`, `M11:PASS`.
- Hồi quy M07 **2/2**, M08 **2/2**, M09 **3/3**, M10 **3/3 PASS**. Guest dùng image snapshot, firmware VARS riêng, kiểm tra hash protected inputs trước/sau và cleanup không cưỡng bức.
- Toolchain report ghi GCC/G++ cross 12.2.0, GNU LD 2.39, NASM 2.15.05; ELF x86_64 ET_EXEC có 3 load segments. Reports chi tiết ở `out/m11/` và build manifest trong `build/m11-guest/` (local, không commit).
- Docs validator **PASS**, unittest tài liệu **39/39 PASS**, `git diff --check` **PASS**. Bốn GitHub workflows **NOT RUN** trên SHA thay đổi; PC thật và firmware ngoài profile QEMU **NOT RUN**.

## Tiếp theo

Chạy docs validator/unittest, kiểm tra diff và toàn bộ M11 CTest lần cuối. Sau đó cần đưa thay đổi lên review/CI theo quy trình dự án; chỉ khi bốn workflows SUCCESS trên SHA nghiệm thu và người dùng xác nhận hoàn thành riêng mới được đánh dấu DONE hoặc tạo annotated tag `milestone/M11`. Không bắt đầu M12 trong thủ tục M11.
