# Prompt chuyển phiên Codex — Lập kế hoạch M27

> Dùng trong phiên mới. Trước khi khảo sát, người dùng phải chủ động bật **Codex Plan Mode thật** trên giao diện. Nội dung prompt không tự chuyển chế độ.

Bạn là Codex tiếp tục phát triển **Lamix OS**. Làm việc và viết Markdown bằng tiếng Việt rõ ràng; giữ nguyên tên kỹ thuật, API, symbol, đường dẫn và lệnh khi cần.

## Nhiệm vụ phiên mới

Chỉ xác minh và lập kế hoạch cho **M27 — System services và shell/terminal native** theo `ROADMAP.md`. M26 đã `DONE`; M27 đang `PLANNED`, chưa được duyệt triển khai.

Nếu giao diện chưa ở Plan Mode thật, chỉ nhắc tôi chủ động chuyển sang Plan Mode rồi dừng. Sau khi ở Plan Mode, chỉ đọc, xác minh và lập kế hoạch. Không sửa/tạo file dự án, không triển khai code/API/tests/fixtures, không cài dependencies, không chạy build, CTest, QEMU, CI hoặc benchmark; không commit/push/tag, không thay đổi GitHub hoặc milestone status. Kết thúc bằng kế hoạch M27 và quyết định còn mở; dừng chờ tôi duyệt rõ ràng. Không bắt đầu implementation hoặc M28.

## Baseline bàn giao — cần xác minh lại

- Workspace: `/home/lam/projects/LamixOS`; repository chính thức: `https://github.com/duclam2011dz/LamixOS`; remote `origin`; nhánh chính `main`.
- M26 PR #33 merge commit `2809e2155c930a061bb6d96ca4df151d085e0481`; acceptance commit `758f2c9998b8d0ffc85e42f8225835b05d784755`; annotated tag `milestone/M26` object `6a5550fe7473e701f4744625f72a2c1b908291b7` peel về acceptance commit. Xác minh lại workspace/Git/remote/PR/tag/CI; nếu không xác minh được, ghi `NOT RUN` hoặc `chưa xác minh`.
- `LATEST_PROFILE=m26`; profile M25 vẫn chọn được tường minh. M18 giữ `PLANNED`; M27 chưa bắt đầu.
- M26 bổ sung VFS kernel nội bộ, tmpfs root và FAT32 8.3/LFN tại `/data`; file API chỉ dùng bởi kernel test manager. Không có shell/service hay file syscall.
- M26 được nghiệm thu với cross-build, CTest 4/4, guest SeaBIOS/OVMF 6/6, regressions M17/M19–M25, docs validator/unittest và bốn workflow CI xanh trên acceptance SHA. Chi tiết tại `docs/SESSION_HANDOFF.md`, `PLANS.md`, `docs/TESTING.md` và `ROADMAP.md`.

## Quy tắc khảo sát

1. Đọc `AGENTS.md` và mọi `AGENTS.md` có phạm vi áp dụng trước khi làm việc.
2. Đọc `docs/SESSION_HANDOFF.md`, `PLANS.md`, mục M27 trong `ROADMAP.md` và các tài liệu liên quan theo quy tắc repository hiện hành.
3. Kiểm tra code hiện có liên quan process/runtime M23, capability/IPC M22, VFS/FAT32/tmpfs M26, console/input, scheduler và profile/build/CI. Chỉ dùng kiểm tra hẹp, chỉ đọc.
4. `ROADMAP.md` có thẩm quyền về mục tiêu/thứ tự. Không tự đổi baseline, tiêu chí nghiệm thu hay phạm vi M28+.
5. Phân biệt baseline đã duyệt, thiết kế đề xuất, quyết định còn mở và phần chưa được triển khai/chứng minh. Không giả định API/service interface nào đã được chốt nếu chưa có tài liệu/code làm căn cứ.

## Phạm vi M27 theo Roadmap

M27 gồm system services và shell/terminal native: init/service lifecycle, console, filesystem utilities, log viewer, command shell căn bản và IPC/service permissions. Nghiệm thu theo Roadmap: chạy lệnh, thao tác file, xử lý lỗi và reboot sạch trong QEMU. Lập kế hoạch trong phạm vi này; không kéo phạm vi M28 compositor/window manager, không thêm mục tiêu chưa được duyệt.

## Đầu ra

Đề xuất kiến trúc và thứ tự triển khai M27; ranh giới kernel/user services, lifecycle, quyền IPC, console/terminal, lệnh và filesystem utilities, profile/build, test host/guest, regression M26, provenance, acceptance đo được và rollback. Nêu rõ câu hỏi cần chủ dự án quyết định. Không đóng băng public API hoặc thay Roadmap. Dừng sau kế hoạch và chờ duyệt; không triển khai M27.
