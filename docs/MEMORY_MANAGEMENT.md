# Quản lý bộ nhớ

**Chưa triển khai.** Theo M09–M12 trong [roadmap](../ROADMAP.md): Bitmap PMM → VMM → kernel heap → buddy allocator. Không bắt buộc xóa bitmap bootstrap khi bổ sung buddy.

Input dự kiến là memory map chuẩn hóa của [LBP](BOOT_PROTOCOL.md). Phải bảo vệ vùng firmware/kernel/modules/ACPI/MMIO và kiểm chứng reserved frames, hết bộ nhớ, protection/page faults, allocation và fragmentation ở milestone tương ứng.

Page-table layout, địa chỉ kernel, allocator API, ownership cụ thể và benchmark chưa đặc tả. Không tạo constants hoặc số liệu trong M00.
