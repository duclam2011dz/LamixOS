# Đồ họa

M16 triển khai software framebuffer renderer ở kernel. Đây là renderer nền, chưa phải desktop hoặc graphics driver; M17 phụ trách VirtIO-GPU 2D, desktop/window manager thuộc milestone sau.

UEFI loader không gọi `SetMode`; chỉ phát LBP framebuffer record khi GOP mode hiện hành là packed RGB/BGR 32 bpp hoặc PixelBitMask hợp lệ. BIOS M16 profile gọi VBE và ưu tiên `1024×768×32`, rồi `800×600×32`; mode thiếu hoặc không hợp lệ thì không phát record và tiếp tục boot. Thay đổi này chỉ bật trong Stage2 M16, giữ Stage1, bootstrap budget 32 KiB, core/BSS budget 256 KiB và LBP v1 wire layout.

Renderer dùng backbuffer kernel-owned XRGB8888; framebuffer firmware là frontbuffer thiết bị, không cấp qua PMM. `graphics_present()` chuyển RGB theo channel masks và chỉ sao chép bounding rectangle của các vùng thay đổi, với pitch firmware. Primitives gồm pixel, line, rectangle outline/fill; text dùng bitmap font 8×16 cho ASCII printable (0x20–0x7e). Non-ASCII được thay bằng glyph `?`. Chưa có compositor, cursor, font selection, Unicode shaping hay user ABI.

Framebuffer vắng mặt hoặc backbuffer không cấp phát được thì marker ghi `M16:FRAMEBUFFER:UNAVAILABLE`; kernel giữ đường serial và không báo M16 render PASS. Khi frame render/present thành công, kernel phát checksum của backbuffer và xác minh pixel frontbuffer đã khớp. OVMF/SeaBIOS guest là bằng chứng profile QEMU, không đại diện PC thật.

Software rendering trước; VirtIO-GPU 2D trên QEMU được ưu tiên sớm để quản lý display/resources/scanout, nguồn pixel vẫn từ software renderer. Không đồng nhất VirtIO-GPU 2D với 3D acceleration.

Virgl/3D là nhánh nghiên cứu có gate Go/No-go, cần xác minh guest protocol/runtime và host QEMU/WSL2. Chưa có kết quả tương thích hoặc FPS.

Desktop compositor/window manager ở user-space, Gaming Glass/Aero Glass dark theme; ưu tiên frame pacing, software fallback và chế độ giảm hiệu ứng khi chơi game. Buffer interfaces, pixel formats và scheduling chưa đặc tả.
