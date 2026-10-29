# Đồ họa

**Renderer/desktop chưa triển khai.** M06 có thể chuyển GOP framebuffer hiện hành cho kernel probe; đây chưa phải renderer hoặc graphics driver. M16–M18 và M28 trong [roadmap](../ROADMAP.md) định hướng phần đồ họa sản phẩm.

EFI loader không gọi `SetMode`; chỉ phát record khi GOP đang chọn RGB/BGR 32 bpp hoặc PixelBitMask hợp lệ, pitch/range căn trang và địa chỉ nằm trong giới hạn page-table x86-64. Khi phát record, LBP Entry có identity mapping RW/NX để kernel truy cập framebuffer. `PixelBltOnly`, format/range không hỗ trợ hoặc feature không có thì bỏ record; kernel ELF yêu cầu framebuffer sẽ bị từ chối. Kiểm thử hiện tại dùng GOP của OVMF, chưa chứng minh firmware PC thật.

Software rendering trước; VirtIO-GPU 2D trên QEMU được ưu tiên sớm để quản lý display/resources/scanout, nguồn pixel vẫn từ software renderer. Không đồng nhất VirtIO-GPU 2D với 3D acceleration.

Virgl/3D là nhánh nghiên cứu có gate Go/No-go, cần xác minh guest protocol/runtime và host QEMU/WSL2. Chưa có kết quả tương thích hoặc FPS.

Desktop compositor/window manager ở user-space, Gaming Glass/Aero Glass dark theme; ưu tiên frame pacing, software fallback và chế độ giảm hiệu ứng khi chơi game. Buffer interfaces, pixel formats và scheduling chưa đặc tả.
