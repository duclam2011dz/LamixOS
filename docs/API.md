# Chỉ mục API và ABI

LBP v1 M03 có [headers C/C++](../include/lamix/boot/lbp.h), [API thư viện C](../include/lamix/boot/contract.h) và [đặc tả](BOOT_PROTOCOL.md). Thư viện dùng buffers của caller, validation/normalization thuần; chưa có loader/kernel hoặc boot đã kiểm chứng. On-wire structs khác với API native (`lbp_span`, `lbp_view`, status enum).

| Giao diện | Nguồn |
|---|---|
| LBP records/layout/kernel note/entry contract | [BOOT_PROTOCOL](BOOT_PROTOCOL.md), [LBP_LAYOUT](LBP_LAYOUT.md) |
| GPT byte-array structs và host model | [IMAGE_LAYOUT](IMAGE_LAYOUT.md) |
| Syscall ABI Lamix-specific x86_64 | Chưa triển khai; M21 trong [roadmap](../ROADMAP.md) |
| Handles/capabilities và IPC | Chưa triển khai; M22 |
| Static ELF64 và user runtime | Chưa triển khai; M23 |
| Lamix 2D SDK | Chưa triển khai; milestones gaming |

Ownership, lifetime, validation và status của LBP đã đặc tả trong BOOT_PROTOCOL; không suy ra các ABI còn lại đã được đóng băng.
