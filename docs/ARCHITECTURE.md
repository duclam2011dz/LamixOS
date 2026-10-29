# Kiến trúc

**Phần lớn subsystem chưa triển khai.** Kernel bootstrap M08 đã DONE; Bitmap PMM M09 đang VERIFYING. VMM, heap, buddy allocator và kernel sản phẩm chưa triển khai. Baseline kiến trúc được duyệt nằm ở [roadmap, mục 1](../ROADMAP.md), tài liệu này là bản đồ thành phần chứ không khẳng định các subsystem bên dưới đã có.

Bootloader BIOS và UEFI tự viết → LBP/BootInfo chuẩn hóa → một kernel entry chung. Hybrid Kernel cung cấp quản lý bộ nhớ, scheduler, processes/threads, capabilities, IPC và syscalls. Duy trì cả kernel-mode drivers và user-mode drivers lâu dài; không chuyển toàn bộ driver ra user-space.

User-space gồm services, compositor/window manager, SDK và games native. Storage đi từ VFS/FAT32/tmpfs đến LamFS; audio, networking, packaging và A/B updates theo milestone muộn trong roadmap.

BSP-only trước SMP; PIC bootstrap → APIC/IOAPIC → MSI/MSI-X. Scheduler khởi đầu preemptive priority-based round-robin. Chi tiết chưa đặc tả sẽ được chốt tại milestone tương ứng, không suy diễn thành ABI.

Xem [boot](BOOT_PROTOCOL.md), [bộ nhớ](MEMORY_MANAGEMENT.md), [đồ họa](GRAPHICS.md), [API](API.md) và [bảo mật](SECURITY.md).
