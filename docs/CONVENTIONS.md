# Quy ước

## Tài liệu

Viết tiếng Việt rõ ràng; giữ tên API, CPU instructions, symbols và paths khi chính xác hơn. Dùng UTF-8, heading rõ, link tương đối tới nguồn sự thật. Mỗi file một trách nhiệm. Phân biệt định hướng, đặc tả đã duyệt và kết quả đã chạy; dùng “Chưa triển khai” cho subsystem chưa tồn tại.

## Mã nguồn sau M00

**Chưa triển khai.** Theo M01 và các milestone kỹ thuật trong [roadmap](../ROADMAP.md): NASM, C, freestanding C++; phân biệt build host với target. Không giả định libstdc++, exceptions, RTTI hoặc dynamic initialization hoạt động. Ownership và lifetime phải rõ khi thiết kế API.

Chưa có cấu hình formatter, compiler flags hoặc quy ước ABI để áp dụng. Chốt chúng khi có toolchain/mã tương ứng; không tự thêm trong M00.
