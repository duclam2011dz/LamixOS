# Quản lý bộ nhớ

**PMM/VMM/heap chưa triển khai.** Theo M09–M12 trong [roadmap](../ROADMAP.md): Bitmap PMM → VMM → kernel heap → buddy allocator. Không bắt buộc xóa bitmap bootstrap khi bổ sung buddy.

M03 đã có memory map contract và thư viện normalization E820/UEFI thuần tại [BOOT_PROTOCOL](BOOT_PROTOCOL.md). Input maps chuẩn hóa theo page4 KiB, bảo vệ BAD/runtime/ACPI/NVS/reservations và không cấp phát từ holes. Host tests không chứng minh loader phát map đúng hoặc PMM bảo vệ frames thật.

Kernel nhận BootInfo identity RO, stack identity RW, kernel higher-half; allocations bootstrap dưới4 GiB, memory map vẫn chứa RAM cao hơn. Page-table construction thuộc M05/M06, VMM address space đầy đủ thuộc M10. Allocator APIs/benchmarks và buddy policy chưa đặc tả; kernel phải giữ ownership/release conditions M03 khi bổ sung PMM.
