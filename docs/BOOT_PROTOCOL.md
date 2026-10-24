# Lamix Boot Protocol

**Chưa triển khai; chưa có đặc tả ABI.** M03 trong [roadmap](../ROADMAP.md) sẽ chốt LBP v1 và GPT image layout.

Baseline yêu cầu bootloader BIOS và UEFI x86_64 tự viết, một kernel entry chung, BootInfo chuẩn hóa và protocol có phiên bản. Disk gồm Protective MBR, BIOS Boot Partition, ESP FAT32, System A/B và User Data; không ghi đè GPT metadata.

M03 cần quyết định magic/version/size, memory descriptor version/stride, framebuffer, ACPI RSDP, kernel/modules, boot device, ownership/lifetime, register/stack/mapping state và xử lý trường thiếu/version mismatch. Chưa ấn định giá trị hoặc layout trong M00.

M04–M07 triển khai và kiểm chứng boot paths. UEFI cần PE/COFF, firmware calling convention, GetMemoryMap/ExitBootServices đúng; BIOS cần transition 16/32/64-bit và bảo vệ vùng reserved. Đây là ràng buộc, chưa phải kết quả test.
