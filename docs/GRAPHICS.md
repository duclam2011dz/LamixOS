# Đồ họa

**Chưa triển khai.** M16–M18 và M28 trong [roadmap](../ROADMAP.md) định hướng renderer và desktop.

Software rendering trước; VirtIO-GPU 2D trên QEMU được ưu tiên sớm để quản lý display/resources/scanout, nguồn pixel vẫn từ software renderer. Không đồng nhất VirtIO-GPU 2D với 3D acceleration.

Virgl/3D là nhánh nghiên cứu có gate Go/No-go, cần xác minh guest protocol/runtime và host QEMU/WSL2. Chưa có kết quả tương thích hoặc FPS.

Desktop compositor/window manager ở user-space, Gaming Glass/Aero Glass dark theme; ưu tiên frame pacing, software fallback và chế độ giảm hiệu ứng khi chơi game. Buffer interfaces, pixel formats và scheduling chưa đặc tả.
