# Mục tiêu Lamix OS

Nguồn thẩm quyền: [ROADMAP.md](ROADMAP.md), đặc biệt mục 0, 1 và 4. Đây là mục tiêu, chưa phải tính năng đã triển khai.

## Mục tiêu

Xây dựng OS Gaming native x86_64 từ đầu với bootloader BIOS/UEFI riêng, Hybrid Kernel, desktop Gaming Glass user-space và nhiều thể loại game native. Dùng toolchain trên host; phát triển lâu dài qua milestone có bằng chứng nghiệm thu.

## Tiêu chí Playable

- BIOS/UEFI boot trên profile hỗ trợ, có logs panic/exception.
- Isolation, threads, syscalls/IPC, storage/VFS và lưu dữ liệu qua reboot được kiểm thử.
- Desktop responsive, input đúng, software rendering fallback; gamepad nếu đã cam kết.
- SDK và nhiều game thuộc các thể loại khác nhau chạy, đóng/mở, lưu tiến độ; Pong chỉ là khởi đầu.
- Intel HDA, networking native và package manager đúng phạm vi phát hành.
- A/B update có recovery được chứng minh, bảo vệ User Data.
- Công bố regression/crash/soak tests và giới hạn hỗ trợ, không bịa hiệu năng hoặc độ ổn định.

## Ngoài mục tiêu ban đầu

Không xây Windows/Linux compatibility layer, không dùng GRUB/Limine làm bootloader chính hoặc lấy kernel Linux làm lõi. Không cam kết 3D trên nhiều GPU, trình duyệt hoàn chỉnh hoặc thương mại hóa ở giai đoạn đầu. Không hứa hoàn thiện OS sau vài lượt chat.
