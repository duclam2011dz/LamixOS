# Chỉ mục API và ABI

LBP v1 M03 có [headers C/C++](../include/lamix/boot/lbp.h), [API thư viện C](../include/lamix/boot/contract.h) và [đặc tả](BOOT_PROTOCOL.md). Thư viện dùng buffers của caller, validation/normalization thuần; M05 có BIOS loader, M06 EFI loader, M07 parity probe; kernel bootstrap M08, PMM M09, VMM BSP M10, kernel heap M11, Buddy PMM thử nghiệm M12, ACPI/PCI/device framework M13, input PS/2 M15 đã DONE; renderer software M16 đang IN_PROGRESS. API M13/M15/M16 dưới đây là nội bộ, không phải public ABI. Chưa có process address spaces hoặc kernel sản phẩm. On-wire structs khác với API native (`lbp_span`, `lbp_view`, status enum).

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

Kernel dùng entry C/NASM qua LBP v1 và gọi `lbp_validate()` trước khi sử dụng BootInfo. GDT/IDT/TSS, serial panic và PIC/PIT self-test thuộc M08. M09 thêm API PMM nội bộ trong `kernel/pmm.h`: `pmm_init`, `pmm_alloc_frame`, `pmm_free_frame`; đây không phải ABI LBP hay API kernel public. M10 thêm API VMM nội bộ trong `kernel/vmm.h`: `vmm_init`, `vmm_map_range`, `vmm_unmap_range`, `vmm_protect_range`, `vmm_query`, `vmm_destroy`; callback truy cập page-table frame là scratch mapping, leaf-frame ownership vẫn thuộc caller. M11 cung cấp C API heap nội bộ trong `kernel/heap.h`: `kheap_init`, `kheap_alloc`, `kheap_calloc`, `kheap_free`, `kheap_get_stats`; không thêm global `new/delete`. M12 hoàn tất API Buddy thử nghiệm nội bộ `pmm_buddy_init`, `pmm_buddy_alloc_order`, `pmm_buddy_free_order`, `pmm_buddy_alloc_frame`, `pmm_buddy_free_frame`, `pmm_buddy_get_stats`; order 0 wrapper là frame 4 KiB, tối đa order 18. Bitmap API hiện hữu không đổi và vẫn backend mặc định. M13 thêm API nội bộ: `acpi_discover`/`acpi_madt_entry_at`, `pci_parse_mcfg`/`pci_enumerate_mcfg`/`pci_probe_bars`/`pci_map_bar`, `device_registry_init`/`device_register_pci`/`device_dma_acquire`/`device_dma_release`. Caller sở hữu scratch/storage; DMA API chỉ giữ owner/generation, không cấp buffer hoặc khởi động DMA. Interfaces không phải public ABI. Chưa có timer API, scheduler hay process address spaces. Chi tiết nằm trong [quản lý bộ nhớ](MEMORY_MANAGEMENT.md), [TESTING M13](TESTING.md#acpi-pci-device-framework-m13) và [handoff](SESSION_HANDOFF.md).


## Input PS/2 M15 (nội bộ kernel)

`kernel/input.h` định nghĩa `input_event`, keycode, modifier, `input_state`, `input_stats` cùng `input_state_init`, `input_keyboard_scancode`, `input_mouse_byte`, `input_poll`, `input_get_stats`. Queue SPSC 128 phần tử dành cho một consumer kernel; sequence tăng đơn điệu, khi đầy drop event mới và tăng `dropped_events`. Keyboard dùng Set 1 đã dịch và ASCII US QWERTY; mouse dùng packet 3 byte, `dy` dương hướng xuống. Đây không phải syscall hoặc user ABI. `kernel/ps2.h` cung cấp `ps2_initialize` callback-based và `ps2_read_irq_byte`; API chỉ dùng trong profile M15.

## Software renderer M16 (nội bộ kernel, DONE)

`kernel/graphics.h` khai báo `graphics_surface`, `graphics_rect` và các thao tác `graphics_init`, `graphics_clear`, `graphics_pixel`, `graphics_fill_rect`, `graphics_rect_outline`, `graphics_line`, `graphics_text`, `graphics_present`, `graphics_verify_present`, `graphics_checksum`. Backbuffer XRGB8888 do caller cấp; renderer chỉ biết pointer/frontbuffer metadata, không tự cấp frame hoặc thay đổi LBP. API không phải public ABI và chưa ổn định ngoài M16.

### VirtIO-GPU M17

`kernel/virtio_gpu.h` cung cấp interface nội bộ `virtio_gpu_initialize`, `virtio_gpu_present`, `virtio_gpu_shutdown` và `virtio_gpu_queue_init`/`virtio_gpu_submit`. `virtio_gpu_shutdown` trả true chỉ khi status reset về 0; caller chỉ được thu hồi queue/backing sau xác nhận này. `virtio_gpu_ops` nối common configuration, notify, và ánh xạ con trỏ kernel sang địa chỉ DMA vật lý; `virtio_gpu_memory` nhận vùng queue/command do kernel sở hữu. Đây không phải public ABI. Profile M17 chỉ thương lượng `VIRTIO_F_VERSION_1`, dùng control queue cho resource 2D; không bật virgl/3D hoặc MSI. Thiếu thiết bị, feature hay scanout phù hợp thì renderer dùng framebuffer LBP nếu có.
