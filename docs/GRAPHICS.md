# Đồ họa

M16 cung cấp software framebuffer renderer ở kernel. M17 dùng renderer này làm nguồn pixel cho VirtIO-GPU 2D; đây chưa phải desktop hoặc compositor, thuộc milestone sau.

UEFI loader không gọi `SetMode`; chỉ phát LBP framebuffer record khi GOP mode hiện hành là packed RGB/BGR 32 bpp hoặc PixelBitMask hợp lệ. BIOS M16 profile gọi VBE và ưu tiên `1024×768×32`, rồi `800×600×32`; mode thiếu hoặc không hợp lệ thì không phát record và tiếp tục boot. Thay đổi này chỉ bật trong Stage2 M16, giữ Stage1, bootstrap budget 32 KiB, core/BSS budget 256 KiB và LBP v1 wire layout.

Renderer dùng backbuffer kernel-owned XRGB8888; framebuffer firmware là frontbuffer thiết bị, không cấp qua PMM. `graphics_present()` chuyển RGB theo channel masks và chỉ sao chép bounding rectangle của các vùng thay đổi, với pitch firmware. Primitives gồm pixel, line, rectangle outline/fill; text dùng bitmap font 8×16 cho ASCII printable (0x20–0x7e). Non-ASCII được thay bằng glyph `?`. Chưa có compositor, cursor, font selection, Unicode shaping hay user ABI.

## VirtIO-GPU 2D M17

Profile `m17-kernel` ưu tiên VirtIO-GPU PCI hiện đại, giữ software renderer M16 làm nguồn pixel. Driver kiểm tra vendor capabilities, BAR, queue và `VIRTIO_F_VERSION_1`; control queue thực hiện display-info, resource create/attach backing, scanout, transfer và flush. Pixel format của resource là `B8G8R8X8_UNORM`, tương thích byte order của backbuffer XRGB8888 trên x86. Scanout dùng kích thước được thiết bị báo, không vượt kích thước render buffer.

Queue và resource backing nằm trong bộ nhớ kernel/heap có địa chỉ DMA vật lý dưới 4 GiB theo giới hạn PMM hiện tại; resource backing được mô tả theo các page rời. Reset thiết bị phải được xác nhận trước khi thu hồi backing; nếu reset timeout, kernel giữ nguyên vùng backing và không dùng lại buffer. Nếu device/feature/scanout không dùng được, renderer trở về framebuffer LBP M16; nếu framebuffer cũng thiếu, kernel giữ serial-only. Profile M08–M16 không đổi. M17 không bật 3D, virgl, cursor queue, MSI hay IOMMU.

QEMU test dùng một raw GPT image chung cho SeaBIOS `pc` và OVMF `q35`. Harness QMP chụp `screen.ppm` khi guest đã present frame, chuyển thành `screen.png` để review; ảnh và logs ở `out/m17/runs/`. Các ảnh guest là bằng chứng riêng, không thay serial markers, kiểm tra protocol hay boot thật. Image chạy thử ở `out/m17/lamix-m17-<kernel-hash-prefix>.img`; lệnh mở trực tiếp được ghi trong [TESTING](TESTING.md#virtio-gpu-2d-m17).

Framebuffer vắng mặt hoặc backbuffer không cấp phát được thì marker ghi `M16:FRAMEBUFFER:UNAVAILABLE`; kernel giữ đường serial và không báo M16 render PASS. Khi frame render/present thành công, kernel phát checksum của backbuffer và xác minh pixel frontbuffer đã khớp. OVMF/SeaBIOS guest là bằng chứng profile QEMU, không đại diện PC thật.

Software rendering trước; VirtIO-GPU 2D trên QEMU được ưu tiên sớm để quản lý display/resources/scanout, nguồn pixel vẫn từ software renderer. Không đồng nhất VirtIO-GPU 2D với 3D acceleration.

Virgl/3D là nhánh nghiên cứu có gate Go/No-go, cần xác minh guest protocol/runtime và host QEMU/WSL2. Chưa có kết quả tương thích hoặc FPS.

Desktop compositor/window manager ở user-space, Gaming Glass/Aero Glass dark theme; ưu tiên frame pacing, software fallback và chế độ giảm hiệu ứng khi chơi game. Buffer interfaces, pixel formats và scheduling chưa đặc tả.
