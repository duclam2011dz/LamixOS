# Kiến trúc

**Phần lớn subsystem chưa triển khai.** Kernel bootstrap M08, Bitmap PMM M09, VMM BSP M10, kernel heap M11 và Buddy PMM thử nghiệm M12 đã DONE. M12 giữ bitmap làm backend mặc định. M13 đang được triển khai: parser ACPI, PCI enumeration/MMIO và device registry đã có trong working tree nhưng chưa nghiệm thu; guest ACPI chưa được kiểm chứng. Chưa có process address spaces hay kernel sản phẩm. Baseline kiến trúc nằm ở [roadmap, mục 1](../ROADMAP.md); trạng thái này không khẳng định subsystem M13 đã DONE.

Bootloader BIOS và UEFI tự viết → LBP/BootInfo chuẩn hóa → một kernel entry chung. Hybrid Kernel cung cấp quản lý bộ nhớ, scheduler, processes/threads, capabilities, IPC và syscalls. Duy trì cả kernel-mode drivers và user-mode drivers lâu dài; không chuyển toàn bộ driver ra user-space.

User-space gồm services, compositor/window manager, SDK và games native. Storage đi từ VFS/FAT32/tmpfs đến LamFS; audio, networking, packaging và A/B updates theo milestone muộn trong roadmap.

BSP-only trước SMP; PIC bootstrap → APIC/IOAPIC → MSI/MSI-X. Scheduler khởi đầu preemptive priority-based round-robin. Chi tiết chưa đặc tả sẽ được chốt tại milestone tương ứng, không suy diễn thành ABI.

Xem [boot](BOOT_PROTOCOL.md), [bộ nhớ](MEMORY_MANAGEMENT.md), [đồ họa](GRAPHICS.md), [API](API.md) và [bảo mật](SECURITY.md).
