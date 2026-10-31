# Kiến trúc

**Phần lớn subsystem chưa triển khai.** Kernel bootstrap M08, Bitmap PMM M09, VMM BSP M10, kernel heap M11, Buddy PMM thử nghiệm M12 và ACPI/PCI/device framework cơ bản M13 đã DONE. M12 giữ bitmap làm backend mặc định. M13 host parser tests và guest PCI/BAR/MMIO PASS; firmware guest không cấp RSDP trong LBP nên ACPI traversal guest NOT RUN. Chưa có process address spaces hay kernel sản phẩm. Baseline kiến trúc nằm ở [roadmap, mục 1](../ROADMAP.md).

Bootloader BIOS và UEFI tự viết → LBP/BootInfo chuẩn hóa → một kernel entry chung. Hybrid Kernel cung cấp quản lý bộ nhớ, scheduler, processes/threads, capabilities, IPC và syscalls. Duy trì cả kernel-mode drivers và user-mode drivers lâu dài; không chuyển toàn bộ driver ra user-space.

User-space gồm services, compositor/window manager, SDK và games native. Storage đi từ VFS/FAT32/tmpfs đến LamFS; audio, networking, packaging và A/B updates theo milestone muộn trong roadmap.

BSP-only trước SMP; PIC bootstrap → APIC/IOAPIC → MSI/MSI-X. Scheduler khởi đầu preemptive priority-based round-robin. Chi tiết chưa đặc tả sẽ được chốt tại milestone tương ứng, không suy diễn thành ABI.

Xem [boot](BOOT_PROTOCOL.md), [bộ nhớ](MEMORY_MANAGEMENT.md), [đồ họa](GRAPHICS.md), [API](API.md) và [bảo mật](SECURITY.md).
