# Prompt chuyển phiên Codex — Lập kế hoạch M26

> Dùng trong một phiên Codex mới. Trước khi khảo sát M26, người dùng phải chủ động bật **Codex Plan Mode thật** trên giao diện. Prompt này và `AGENTS.md` không tự đổi chế độ UI.

Bạn là Codex tiếp tục phát triển **Lamix OS**. Làm việc và viết Markdown bằng tiếng Việt rõ ràng; giữ nguyên tên kỹ thuật, API, symbol, đường dẫn và lệnh khi cần.

## Nhiệm vụ phiên mới

Chỉ khảo sát và lập kế hoạch cho **M26 — VFS + tmpfs + FAT32** theo `ROADMAP.md`. M25 đã `DONE`; M26 vẫn `PLANNED`, chưa được duyệt triển khai.

Nếu giao diện chưa ở Plan Mode thật, chỉ nhắc tôi chủ động chuyển sang Plan Mode rồi dừng. Sau khi ở Plan Mode, chỉ đọc, xác minh và lập kế hoạch; không sửa/tạo file dự án, triển khai code/API/tests/fixtures, cài dependencies, chạy build, CTest, QEMU, CI hoặc benchmark, commit/push/tag, thay đổi GitHub hoặc milestone status. Kết thúc bằng kế hoạch M26 và các quyết định còn mở; sau đó dừng chờ tôi duyệt rõ ràng. Không bắt đầu M26 implementation hoặc M27.

## Baseline bàn giao — cần xác minh lại

- Workspace: `/home/lam/projects/LamixOS`; repository chính thức: `https://github.com/duclam2011dz/LamixOS`; nhánh chính `main`; remote `origin`.
- M25 acceptance commit `1f6241935d776c64f578af2fc60f55f05ad5f061`; PR #31 merge commit `53b3556dd91ae96f2eca5dcc5635be513d90e66e`; annotated tag `milestone/M25` object `04b34ba058c95f464a0e98704fca98f28c7b2045` peel về acceptance commit. Xác minh lại branch, `git status`, HEAD, remote, PR/tag và CI trên SHA nghiệm thu. Không suy diễn nếu dịch vụ không truy cập được; ghi `NOT RUN`/`chưa xác minh`.
- M25 bổ sung block abstraction và VirtIO-Blk trong profile `m25-kernel`, giữ filesystem, syscall lưu trữ và user storage API ngoài phạm vi. `scripts/lamix_qemu.py` đặt `LATEST_PROFILE="m25"`; `--profile m24` vẫn chọn được.
- Bằng chứng M25 được ghi trong `PLANS.md`, `docs/TESTING.md` và `docs/SESSION_HANDOFF.md`: native ASan/UBSan + TSan, cross-build, guest SeaBIOS `pc`/OVMF `q35` × 2 vCPU × 3 lượt, kiểm chứng raw test image; bốn workflow CI SUCCESS trên acceptance SHA và implementation SHA. Xác minh lại acceptance SHA, tag/PR/merge và workflow cuối trước khi dùng làm baseline.
- M18 vẫn `PLANNED`; không được kích hoạt hoặc đổi thứ tự. M27 trở đi ngoài phạm vi lượt plan này.

## Quy tắc khảo sát

1. Đọc `AGENTS.md` và mọi `AGENTS.md` có phạm vi áp dụng trước khi làm việc.
2. Đọc `docs/SESSION_HANDOFF.md`, `PLANS.md`, `ROADMAP.md` (mục M26), `README.md`, `GOALS.md`, `CONTRIBUTING.md`, `CHANGELOG.md`, `MASTER_PROMPT_LAMIX_OS.md`, `PROMPT_NEXT_MILESTONE.md` và `LICENSE`. Liệt kê Markdown dự án rồi đọc hết Markdown theo quy định hiện hành; metadata và toàn nội dung skill do dự án quản lý cần đọc trước khi áp dụng workflow phù hợp.
3. Kiểm tra code và contract block I/O M25, PCI/device framework, quản lý memory, capability/IPC, initramfs, loader/boot images và profile hiện hành để xác định tích hợp VFS đúng phạm vi.
4. `ROADMAP.md` có thẩm quyền về mục tiêu/thứ tự. Không đổi mục tiêu, acceptance hoặc phạm vi M27+.
5. Phân loại rõ baseline đã duyệt, thiết kế M26 đề xuất, quyết định còn cần tôi chốt và phần chưa triển khai/chưa chứng minh. Không đóng băng API hoặc semantics ngoài thẩm quyền Roadmap.

## Phạm vi M26 theo Roadmap

Roadmap mô tả **VFS + tmpfs + FAT32**: path resolution, vnode/file handles, mount semantics, caching ban đầu, FAT32 reader rồi writer, kiểm tra FAT chain và lỗi metadata. Nghiệm thu: create/read/write/delete/rename/mount tests, persistence qua reboot, không nhầm ESP với user data.

Khảo sát prerequisite M25, phân biệt block I/O với filesystem, và đề xuất các interface/ownership/locking/error semantics cần thiết. Xác định cách cô lập disk image, partition, ESP và volume dữ liệu; không ghi disk host thật. Không mở rộng sang GUI, shell/services M27, filesystem sản phẩm khác, storage syscall hoặc thay LBP/boot contract nếu chưa có kế hoạch được duyệt.

## Đầu ra

Trình bày baseline và bằng chứng hiện có, thiết kế/thứ tự bước M26, module/file dự kiến, xử lý FAT32 corruption và giới hạn an toàn ghi, tests host/guest/persistence, profile/CI/provenance, regression M17–M25, rollback và điều kiện acceptance đo được. Nêu các quyết định còn mở. Sau đó dừng chờ duyệt; không triển khai M26.
