# Prompt chuyển phiên Codex — Lập kế hoạch M28

> Trước khi khảo sát, người dùng phải chủ động bật **Codex Plan Mode thật** trên giao diện. Prompt này không tự chuyển chế độ.

Bạn là Codex tiếp tục phát triển **Lamix OS**. Làm việc và viết Markdown bằng tiếng Việt rõ ràng; giữ nguyên tên kỹ thuật, API, symbol, đường dẫn và lệnh khi cần.

## Nhiệm vụ phiên mới

Chỉ xác minh và lập kế hoạch cho **M28 — User-space desktop compositor + window manager** theo `ROADMAP.md`. M27 đã `DONE`; M28 vẫn `PLANNED`, chưa được duyệt triển khai.

Nếu giao diện chưa ở Plan Mode thật, chỉ nhắc tôi chủ động chuyển sang Plan Mode rồi dừng. Sau khi vào Plan Mode, chỉ đọc, xác minh và lập kế hoạch. Không sửa/tạo file dự án, không triển khai code/API/tests/fixtures, không cài dependencies, không chạy build, CTest, QEMU, CI hoặc benchmark; không commit/push/tag, không thay đổi GitHub hay milestone status. Kết thúc bằng kế hoạch M28 và các quyết định còn mở; dừng chờ tôi duyệt rõ ràng. Không bắt đầu M28 implementation hoặc M29.

## Baseline bàn giao — cần xác minh lại

- Workspace: `/home/lam/projects/LamixOS`; repo chính thức: `https://github.com/duclam2011dz/LamixOS`; remote `origin`; nhánh chính `main`.
- M27 acceptance SHA `b6c2eecbd129286e3c16e3c15c35dbdf02c22e28`; PR #35 merge commit `4e6e45d52c110e0bf90af1b756da981f1176a2d9`; annotated tag `milestone/M27` object `dab60063ca67288224a139ae43a12177fb19122d` peel về acceptance SHA. Xác minh lại Git, PR, tag và CI; nếu không truy cập được, ghi `NOT RUN` hoặc `chưa xác minh`.
- `LATEST_PROFILE=m27`; profile M26 và cũ hơn vẫn chọn tường minh. M18 vẫn `PLANNED`.
- M27 thêm init/service manager, shell/terminal, wrappers ABI 1.2, manifest rights subset, file/log/service/reboot và clean-reboot FAT32 persistence; chi tiết/bằng chứng trong `PLANS.md`, `ROADMAP.md`, `docs/TESTING.md` và `docs/SESSION_HANDOFF.md`.
- Người dùng đã miễn review PR #35 vì repository chỉ có một maintainer; không có review độc lập. Đây không phải security audit.

## Quy tắc khảo sát

1. Đọc `AGENTS.md` và mọi `AGENTS.md` có phạm vi trước khi làm.
2. Đọc `docs/SESSION_HANDOFF.md`, `PLANS.md`, mục M28 trong `ROADMAP.md` và tài liệu liên quan theo quy tắc repository.
3. Kiểm tra code hiện hành liên quan compositor/rendering M16/M17, input M15, capability/IPC M22, process/runtime M23, SMP M24, services/shell M27, profile/build/CI. Chỉ dùng kiểm tra hẹp, chỉ đọc.
4. `ROADMAP.md` là thẩm quyền về mục tiêu/thứ tự. Không tự đổi baseline, acceptance hoặc phạm vi M29+.
5. Phân biệt baseline đã duyệt, thiết kế đề xuất, quyết định còn mở và phần chưa được chứng minh. Không giả định compositor API nào đã tồn tại nếu chưa có bằng chứng.

## Phạm vi M28 theo Roadmap

Roadmap mô tả user-space desktop compositor/window manager: shared buffers/IPC, windows/surfaces, mouse/keyboard routing, frame scheduling, transparency/shadows/blur theo tầng, shell/taskbar/launcher, dark neon Gaming Glass và performance mode. Nghiệm thu: nhiều cửa sổ, resize/focus/input đúng, UI responsive trên software renderer/VirtIO-GPU 2D.

Lập kế hoạch trong đúng phạm vi M28. Phân tích boundary user/kernel và quyền shared buffers, lifecycle window/surface, compositor scheduling, input routing, rendering/fallback và tiêu chí responsiveness đo được. Không kéo M29 SDK/game, không đổi LBP v1/boot contract, profile cũ hoặc baseline kiến trúc đã duyệt.

## Đầu ra

Đưa ra baseline/bằng chứng, thiết kế và thứ tự triển khai M28, module/profile dự kiến, host/guest tests, regressions M17/M19–M27, CI/provenance, acceptance đo được, rủi ro và rollback. Nêu quyết định còn cần chủ dự án chốt. Không đóng băng public API hoặc mở rộng Roadmap. Sau đó dừng chờ duyệt; không triển khai M28.
