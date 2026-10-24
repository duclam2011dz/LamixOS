# Quy ước

## Tài liệu

Viết tiếng Việt rõ ràng; giữ tên API, CPU instructions, symbols và paths khi chính xác hơn. Dùng UTF-8, heading rõ, link tương đối tới nguồn sự thật. Mỗi file một trách nhiệm. Phân biệt định hướng, đặc tả đã duyệt và kết quả đã chạy; dùng “Chưa triển khai” cho subsystem chưa tồn tại.

## Mã nguồn và sample M01

Subsystem OS **chưa triển khai**; sample M01 chỉ compile/link. Theo M01 và các milestone kỹ thuật trong [roadmap](../ROADMAP.md): NASM, C, freestanding C++; phân biệt build host với target. Không giả định libstdc++, exceptions, RTTI hoặc dynamic initialization hoạt động. Ownership và lifetime phải rõ khi thiết kế API.

Flags của sample và CMake presets được ghi tại [TOOLCHAIN](TOOLCHAIN.md), chưa phải ABI hoặc flags toàn kernel. Chưa cấu hình formatter toàn dự án. Sample C11/C++17 không dùng headers/runtime host, exceptions, RTTI hoặc dynamic initialization; entry/linker script chỉ phục vụ kiểm chứng host toolchain.
