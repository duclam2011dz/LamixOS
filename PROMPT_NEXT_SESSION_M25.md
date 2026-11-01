# Prompt chuyển phiên Codex — Lập kế hoạch M25

> Dùng trong một phiên Codex mới. Trước khi khảo sát M25, người dùng phải chủ động bật **Codex Plan Mode thật** trên giao diện. Prompt này và `AGENTS.md` không tự đổi chế độ UI.

Bạn là Codex tiếp tục phát triển **Lamix OS**. Làm việc và viết Markdown bằng tiếng Việt rõ ràng; giữ nguyên tên kỹ thuật, API, symbol, đường dẫn và lệnh khi cần.

## Nhiệm vụ phiên mới

Chỉ khảo sát và lập kế hoạch cho **M25 — Block I/O + VirtIO-Blk** theo `ROADMAP.md`. M24 đã `DONE`; M25 vẫn `PLANNED`, chưa được duyệt triển khai.

Nếu chưa ở Plan Mode thật, chỉ nhắc người dùng chủ động chuyển sang Plan Mode rồi dừng. Sau khi ở Plan Mode, chỉ đọc, xác minh và lập kế hoạch; không sửa/tạo file dự án, triển khai code/API/tests/fixtures, cài dependencies, commit/push/tag, thay đổi GitHub hoặc milestone status. Không chạy build, CTest, QEMU, CI hay benchmark. Kết thúc bằng kế hoạch M25, các quyết định còn mở và dừng chờ duyệt rõ ràng. Không bắt đầu M25 implementation hoặc M26.

## Baseline bàn giao đã biết — cần xác minh lại

- Repository chính thức: `https://github.com/duclam2011dz/LamixOS`; workspace: `/home/lam/projects/LamixOS`.
- M24 acceptance commit: `ecbeef72846e473e1b8e0cb572a086bd7e7c5dc9`; PR #28 merge commit: `590ebae8eacaa1f0a582051e67bae750a717bdcc`.
- Annotated tag `milestone/M24` object: `7dd2b905a08e93320146cd041ed28c033b813b28`, trỏ acceptance commit.
- M24: AP startup qua MADT xAPIC/INIT-SIPI, per-CPU state/scheduler/affinity/work stealing và TLB shootdown trong profile riêng `m24-kernel`. Profile BSP-only, `latest`/`LATEST_PROFILE=m20`, LBP v1, syscall ABI, M18 `PLANNED` vẫn giữ nguyên.
- Bốn workflow PR #28 đã SUCCESS; Toolchain CI run `41313786645` thành công ở attempt 2 sau một assertion marker failure ở attempt 1. Chi tiết kết quả, rủi ro và lệnh ở `docs/SESSION_HANDOFF.md`, `PLANS.md`, `docs/TESTING.md`.
- M25–M44 vẫn `PLANNED`. Chưa bắt đầu M25; không suy diễn thêm storage contract ngoài Roadmap và tài liệu hiện có.

## Quy tắc khảo sát

1. Đọc `AGENTS.md` và các `AGENTS.md` áp dụng; tuân thủ Plan-first và giới hạn milestone.
2. Kiểm tra Git status/branch/HEAD, remote, PR/tag/CI nếu dịch vụ sẵn sàng; giữ nguyên mọi file người dùng chưa theo dõi. Nếu không kiểm tra được, ghi `NOT RUN` hoặc `chưa xác minh`.
3. Đọc `docs/SESSION_HANDOFF.md`, `PLANS.md`, `ROADMAP.md` (đúng phần M25), cùng tài liệu kiến trúc, storage, testing và quy ước có liên quan. Chỉ đọc thêm file cần để kiểm tra phụ thuộc kỹ thuật thực tế.
4. `ROADMAP.md` là thẩm quyền về mục tiêu và thứ tự milestone. Không tự đổi baseline, acceptance hoặc phạm vi M26+.
5. Phân biệt rõ baseline đã được duyệt, đề xuất thiết kế M25, quyết định cần người dùng chốt, và phần chưa triển khai/chưa chứng minh. Không đóng băng API, layout hoặc acceptance thay đổi trong lượt plan.

## Phạm vi M25 theo Roadmap

Roadmap mô tả **Block I/O + VirtIO-Blk**: storage abstraction, ranh giới request async/sync và đọc/ghi an toàn. Nghiệm thu hiện ghi: read/write/verify dữ liệu trên test image riêng; kiểm thử không ghi lên disk host. Hãy kiểm tra phụ thuộc thật từ M24 và các contract boot/image hiện hành trước khi đề xuất kiến trúc cụ thể. Không triển khai VFS/FAT32, filesystem sản phẩm, user services hoặc M26.

## Đầu ra

Kế hoạch M25 cụ thể gồm mục tiêu, phạm vi/ngoài phạm vi, prerequisite, file/module có thể ảnh hưởng, ownership/lifetime, error/timeout/reset policy, test host/guest và BIOS/UEFI nếu hỗ trợ, CI/profile/provenance, rollback và tiêu chí dừng. Đề xuất acceptance đo được nhưng không sửa Roadmap hoặc baseline. Nêu quyết định còn mở; sau đó dừng chờ phê duyệt triển khai M25.
