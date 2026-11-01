# Kiến trúc

**Phần lớn subsystem chưa triển khai.** Kernel bootstrap M08, Bitmap PMM M09, VMM BSP M10, kernel heap M11, Buddy PMM thử nghiệm M12, ACPI/PCI/device framework cơ bản M13, APIC/timer M14, input M15, software renderer M16, VirtIO-GPU 2D M17 và scheduler BSP M19 đã DONE. M12 giữ bitmap làm backend mặc định; M18 vẫn PLANNED. M20 đang IN_PROGRESS với process address spaces BSP, ring 3 và lifecycle nội bộ; chưa có syscall ABI M21, ELF loader/runtime M23 hay kernel sản phẩm. M13 guest không cấp RSDP trong LBP nên ACPI traversal guest **NOT RUN**. Baseline kiến trúc nằm ở [roadmap, mục 1](../ROADMAP.md).

Bootloader BIOS và UEFI tự viết → LBP/BootInfo chuẩn hóa → một kernel entry chung. Hybrid Kernel cung cấp quản lý bộ nhớ, scheduler, processes/threads, capabilities, IPC và syscalls. Duy trì cả kernel-mode drivers và user-mode drivers lâu dài; không chuyển toàn bộ driver ra user-space.

User-space gồm services, compositor/window manager, SDK và games native. Storage đi từ VFS/FAT32/tmpfs đến LamFS; audio, networking, packaging và A/B updates theo milestone muộn trong roadmap.

BSP-only trước SMP; PIC bootstrap → APIC/IOAPIC → MSI/MSI-X. M19 scheduler BSP đã nghiệm thu preemptive priority round-robin qua LAPIC timer. M20 hiện thêm process một thread/address space riêng và user-mode test payload; không thêm syscall ABI hoặc executable loader. M18 GPU 3D PoC vẫn PLANNED/để dành, không phải dependency. Chi tiết chưa đặc tả sẽ được chốt tại milestone tương ứng, không suy diễn thành ABI.

Xem [boot](BOOT_PROTOCOL.md), [bộ nhớ](MEMORY_MANAGEMENT.md), [đồ họa](GRAPHICS.md), [API](API.md) và [bảo mật](SECURITY.md).
