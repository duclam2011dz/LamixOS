# Kiến trúc

**Phần lớn subsystem chưa triển khai.** Kernel bootstrap M08, Bitmap PMM M09 và VMM BSP M10 đã DONE; kernel heap M11 đang VERIFYING. M10 có kernel address space và thao tác paging 4 KiB; M11 thêm allocator kernel BSP nhưng chưa có buddy allocator, process address spaces hay kernel sản phẩm. Baseline kiến trúc được duyệt nằm ở [roadmap, mục 1](../ROADMAP.md), tài liệu này là bản đồ thành phần chứ không khẳng định các subsystem bên dưới đã có.

Bootloader BIOS và UEFI tự viết → LBP/BootInfo chuẩn hóa → một kernel entry chung. Hybrid Kernel cung cấp quản lý bộ nhớ, scheduler, processes/threads, capabilities, IPC và syscalls. Duy trì cả kernel-mode drivers và user-mode drivers lâu dài; không chuyển toàn bộ driver ra user-space.

User-space gồm services, compositor/window manager, SDK và games native. Storage đi từ VFS/FAT32/tmpfs đến LamFS; audio, networking, packaging và A/B updates theo milestone muộn trong roadmap.

BSP-only trước SMP; PIC bootstrap → APIC/IOAPIC → MSI/MSI-X. Scheduler khởi đầu preemptive priority-based round-robin. Chi tiết chưa đặc tả sẽ được chốt tại milestone tương ứng, không suy diễn thành ABI.

Xem [boot](BOOT_PROTOCOL.md), [bộ nhớ](MEMORY_MANAGEMENT.md), [đồ họa](GRAPHICS.md), [API](API.md) và [bảo mật](SECURITY.md).
