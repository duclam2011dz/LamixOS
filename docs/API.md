# Chỉ mục API và ABI

LBP v1 M03 có [headers C/C++](../include/lamix/boot/lbp.h), [API thư viện C](../include/lamix/boot/contract.h) và [đặc tả](BOOT_PROTOCOL.md). Thư viện dùng buffers của caller, validation/normalization thuần; M05 có BIOS loader, M06 EFI loader, M07 parity probe; kernel bootstrap M08, PMM M09 và VMM BSP M10 đã DONE; kernel heap M11 đang VERIFYING. Chưa có process address spaces hoặc kernel sản phẩm. On-wire structs khác với API native (`lbp_span`, `lbp_view`, status enum).

| Giao diện | Nguồn |
|---|---|
| LBP records/layout/kernel note/entry contract | [BOOT_PROTOCOL](BOOT_PROTOCOL.md), [LBP_LAYOUT](LBP_LAYOUT.md) |
| GPT byte-array structs và host model | [IMAGE_LAYOUT](IMAGE_LAYOUT.md) |
| Syscall ABI Lamix-specific x86_64 | Chưa triển khai; M21 trong [roadmap](../ROADMAP.md) |
| Handles/capabilities và IPC | Chưa triển khai; M22 |
| Static ELF64 và user runtime | Chưa triển khai; M23 |
| Lamix 2D SDK | Chưa triển khai; milestones gaming |

Ownership, lifetime, validation và status của LBP đã đặc tả trong BOOT_PROTOCOL; không suy ra các ABI còn lại đã được đóng băng.

## Loader bootstrap M05

API native [loader.h](../include/lamix/boot/loader.h): reader read_at, GPT/FAT32 reader, ELF inspection plan và ELF load bằng allocator callback. Buffers/context do caller sở hữu; không wire format hoặc firmware callbacks trong LBP. Public structs và limits tại [BIOS_STAGE2](BIOS_STAGE2.md#parsers-và-api-dùng-chung). Các output chỉ publish khi validation thành công; scratch có thể đổi khi lỗi. Core BIOS32 thực hiện allocation/copy/zero theo plan, không cung cấp kernel allocator runtime.

## Bổ sung GPT identity M06

`lamix_gpt_open_uefi(reader, esp_start, esp_sectors, scratch, capacity, out)` dùng chung GPT parser nhưng đối chiếu ESP extent tìm từ Device Path/Loaded Image với bảng GPT. `lamix_gpt_open` BIOS tiếp tục đối chiếu BIOS Boot extent; cả hai kiểm tra cùng metadata/GUID/layout. API không nhận firmware handles và không đưa chúng vào LBP. EFI implementation nằm trong [boot/uefi](../boot/uefi/loader.c); PE/COFF được build độc lập với Clang+LLD.

## Kernel bootstrap M08

Kernel dùng entry C/NASM qua LBP v1 và gọi `lbp_validate()` trước khi sử dụng BootInfo. GDT/IDT/TSS, serial panic và PIC/PIT self-test thuộc M08. M09 thêm API PMM nội bộ trong `kernel/pmm.h`: `pmm_init`, `pmm_alloc_frame`, `pmm_free_frame`; đây không phải ABI LBP hay API kernel public. M10 thêm API VMM nội bộ trong `kernel/vmm.h`: `vmm_init`, `vmm_map_range`, `vmm_unmap_range`, `vmm_protect_range`, `vmm_query`, `vmm_destroy`; callback truy cập page-table frame là scratch mapping, leaf-frame ownership vẫn thuộc caller. M11 cung cấp C API heap nội bộ trong `kernel/heap.h`: `kheap_init`, `kheap_alloc`, `kheap_calloc`, `kheap_free`, `kheap_get_stats`; không thêm global `new/delete`. M11 đang VERIFYING; chưa có timer API, scheduler hay process address spaces. Chi tiết nằm trong [quản lý bộ nhớ](MEMORY_MANAGEMENT.md), [TESTING](TESTING.md#kernel-heap-m11) và [handoff](SESSION_HANDOFF.md).
