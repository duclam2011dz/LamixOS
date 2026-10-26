# Kế hoạch hiện hành — M02

Trạng thái milestone: M00=DONE; M01=DONE; M02=DONE

Người dùng đã duyệt M02 ngày 2026-10-26 trong phiên hiện tại. M00/M01 giữ nguyên tags nghiệm thu; không chuyển phiên hoặc bắt đầu M03.

## Phạm vi và thứ tự

- [x] Runner/QMP/serial, deadline monotonic và process-group cleanup; kết quả JSON cùng logs mỗi run.
- [x] Fixture tests positive/negative và QEMU thật paused BIOS/UEFI; không boot OS.
- [x] CTest host-only, hai boot skeleton SKIPPED (77) vì chưa có image.
- [x] Harness CI riêng, regression M01/tài liệu và ba workflows SUCCESS trên 42116ea2ba92800384b76e3130ff7f6c5b91b01f; commit nghiệm thu cuối tiếp tục phải có ba workflows xanh.
- [x] Người dùng phê duyệt kết thúc M02; annotated milestone/M02 đã tạo/push và xác minh local/remote trên 3a4f6206f04287d07b8bd9f29d5cc941af2f50dc.

Default launch deadline 10 giây, boot 30 giây, cleanup grace 2 giây. BIOS pc/SeaBIOS, UEFI q35 với CODE read-only và bản sao VARS riêng, TCG/1 CPU/256 MiB, headless/network off. Marker literal theo dòng, panic ưu tiên; exit thiếu marker không phải PASS. Image raw phải là regular file và dùng snapshot.

Không tạo bootloader/kernel/image/LBP hoặc chạy sample ELF M01 như guest. Chỉ báo emulator launch PASS; boot skeleton chưa có image phải SKIP, không đổi thành boot PASS. M02 PASS cần fixture tests, launch thật, cleanup, logs/summary và CI thực tế; không hạ tiêu chí khi lỗi.

## Tài liệu và rollback

Cập nhật README/TESTING/TROUBLESHOOTING/CHANGELOG/handoff và trạng thái roadmap, mở rộng validator trạng thái đến M02. Không sửa kiến trúc hoặc thứ tự milestone. Rollback bằng diff/commit tiếp nối, chỉ dọn artifacts thuộc run; không force-push, đổi tags M00/M01 hoặc ghi disk thật.

Trạng thái thật ở [handoff](docs/SESSION_HANDOFF.md); tag M02 đã được duyệt và xác minh, không tự mở M03.

## Bằng chứng và đóng thủ tục

33 fixture tests PASS, 2 QEMU launch-only thật PASS; CTest hai suite thành công và hai boot skeleton SKIPPED/77. Đã đọc artifact GitHub, không có OS boot PASS. Regression Debug/Release/ELF/tái lập M01 và 24 unittest tài liệu PASS. Xem [handoff](docs/SESSION_HANDOFF.md) cho SHA/run URLs và sự cố mirror đã xử lý.

Thủ tục tag M02 đã hoàn tất. Commit tài liệu đóng thủ tục giữ nguyên đích tag, phải được push và có ba workflows SUCCESS trước báo cáo cuối. Chờ chỉ dẫn tiếp theo của người dùng; lượt này không chuyển phiên hoặc bắt đầu M03.
