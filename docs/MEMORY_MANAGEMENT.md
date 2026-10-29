# Quản lý bộ nhớ

**PMM/VMM/heap chưa triển khai.** Theo M09–M12 trong [roadmap](../ROADMAP.md): Bitmap PMM → VMM → kernel heap → buddy allocator. Không bắt buộc xóa bitmap bootstrap khi bổ sung buddy.

M03 đã có memory map contract và thư viện normalization E820/UEFI thuần tại [BOOT_PROTOCOL](BOOT_PROTOCOL.md). Input maps chuẩn hóa theo page4 KiB, bảo vệ BAD/runtime/ACPI/NVS/reservations và không cấp phát từ holes. Host tests không chứng minh loader phát map đúng hoặc PMM bảo vệ frames thật.

Kernel nhận BootInfo identity RO, stack identity RW, kernel higher-half; kernel allocations bootstrap dưới4 GiB, memory map vẫn chứa RAM cao hơn. M06 UEFI map GOP framebuffer identity RW/NX nếu feature hợp lệ, kể cả physical range trên4 GiB; đây là vùng thiết bị theo record framebuffer, không phải kernel allocation. Page-table construction thuộc M05/M06, VMM address space đầy đủ thuộc M10. Allocator APIs/benchmarks và buddy policy chưa đặc tả; kernel phải giữ ownership/release conditions M03 khi bổ sung PMM.

M05 đã có first-fit allocator loader dưới4 GiB, reservations/low1 MiB protection và page tables4 KiB cho handoff; không thay PMM/VMM milestones. Probe walk actual mappings, RO/guard fault tests và RAM>4 GiB kiểm contract; [BIOS_STAGE2](BIOS_STAGE2.md) ghi limits/lifetime, [handoff](SESSION_HANDOFF.md) ghi kết quả thật.
