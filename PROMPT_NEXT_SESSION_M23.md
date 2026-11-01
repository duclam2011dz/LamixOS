# Prompt chuyển phiên Codex — Lập kế hoạch M23

> Gửi prompt này trong một phiên Codex mới. **Trước khi khảo sát M23, hãy chủ động bật Codex Plan Mode thật trên giao diện.** Prompt và `AGENTS.md` không tự chuyển chế độ UI.

Bạn là Codex tiếp tục phát triển **Lamix OS**. Làm việc và viết Markdown bằng tiếng Việt rõ ràng; giữ nguyên tên kỹ thuật, API, symbol, đường dẫn và lệnh khi cần.

## Nhiệm vụ

Chỉ khảo sát và lập kế hoạch cho **M23 — Static ELF64 loader/user runtime** theo `ROADMAP.md`. M22 đã nghiệm thu; M23 đang `PLANNED`, chưa được duyệt triển khai. Không bắt đầu code, API, tests, fixtures hay milestone M24+ trong lượt lập kế hoạch.

## Plan Mode và giới hạn

- Nếu giao diện chưa ở Codex Plan Mode thật, chỉ nhắc tôi chủ động chuyển sang Plan Mode rồi dừng; chưa khảo sát milestone.
- Sau khi ở Plan Mode, chỉ đọc, xác minh và lập kế hoạch. Không sửa/tạo file dự án, triển khai code/API/tests/fixtures, cài dependencies, chạy build/CTest/QEMU/CI/benchmark, commit/push/tag, thay đổi GitHub hoặc milestone status.
- Kết thúc bằng kế hoạch M23 cụ thể, rủi ro và quyết định còn mở; dừng chờ tôi duyệt rõ ràng trước khi triển khai.

## Baseline M22 đã nghiệm thu

- Workspace/repository chính thức: `/home/lam/projects/LamixOS`, `https://github.com/duclam2011dz/LamixOS`; xác minh lại `git status`, branch/HEAD, remote, PR/tag và CI trong phiên mới.
- M22 acceptance SHA `0d2c4b62e860ba336da439abf03c8cd6b421157c`; PR [#24](https://github.com/duclam2011dz/LamixOS/pull/24), merge commit `d2c3be428f69d439caad901e82c738102fede05e`; annotated tag `milestone/M22`, tag object `580ffdf3824102ed2bdb89ddf7e825364a8a00b7` trỏ acceptance SHA.
- Bốn workflow SUCCESS trên acceptance SHA: Toolchain [41277685047](https://github.com/duclam2011dz/LamixOS/actions/runs/41277685047) (attempt 2), Harness [41277685300](https://github.com/duclam2011dz/LamixOS/actions/runs/41277685300), Documentation [41277685046](https://github.com/duclam2011dz/LamixOS/actions/runs/41277685046), Boot Contract [41277685160](https://github.com/duclam2011dz/LamixOS/actions/runs/41277685160). Toolchain attempt đầu fail ở regression M08 UEFI page-fault do serial log thiếu `cr2`; retry job hoàn tất SUCCESS.
- M22 CTest **4/4 PASS**; guest SeaBIOS `pc` và OVMF `q35` **3/3 mỗi firmware PASS**; Toolchain CI chạy regression M17/M19/M20/M21 và M22 thành công. Docs validator **PASS**, unittest tài liệu **41/41 PASS** trước commit tài liệu bàn giao.
- M22 cung cấp handle/capability, channel IPC và SHM trong profile `m22-kernel`; ABI M22 là 1.1, M21 giữ 1.0; `latest` vẫn M20. M22 sửa atomic MOVE/enqueue và ngăn channel reference cycles. LBP v1 và entry ABI giữ nguyên.
- M18 vẫn `PLANNED`/để dành. M24+ không thuộc phạm vi. Snapshot này chỉ gợi ý; nếu dịch vụ không khả dụng, ghi `NOT RUN`/`chưa xác minh`, không suy diễn PASS.

## Tài liệu và khảo sát

Tuân thủ `AGENTS.md` hiện hành và đọc đúng tài liệu cần cho M23, gồm `docs/SESSION_HANDOFF.md`, `PLANS.md`, mục M23 trong `ROADMAP.md`, `README.md`, `GOALS.md`, `CONTRIBUTING.md`, `CHANGELOG.md`, `MASTER_PROMPT_LAMIX_OS.md`, `PROMPT_NEXT_MILESTONE.md`, `LICENSE`, cùng tài liệu kiến trúc/API/bảo mật/kiểm thử liên quan. Đọc các `AGENTS.md` có phạm vi áp dụng trước khi làm việc.

Kiểm tra code hiện có về ELF parsing/loading primitives, M20 process/address-space lifetime, M21/M22 ABI và user payload/profile gates. Phân biệt rõ loader test-only hiện có với executable loader/runtime sản phẩm; không giả định initramfs, filesystem, relocation class, argv/env ABI, `crt0` hoặc libc đã được duyệt nếu repo/roadmap chưa xác nhận.

## Phạm vi M23 theo Roadmap

Roadmap ghi M23 gồm: static ELF64 loader/user runtime, hỗ trợ relocations theo phạm vi, arguments/environment, `crt0`, libc subset tối thiểu và executable format validation. Acceptance: chạy hai user programs độc lập từ disk/initramfs và xử lý ELF hỏng. Giữ nguyên mục tiêu/thứ tự Roadmap; không mở rộng sang filesystem/storage milestones, dynamic linking, package manager hoặc SMP. Đề xuất test và điều kiện acceptance đo được, thứ tự bước, file/module dự kiến, profile/build/CI, hồi quy M20–M22, provenance/report và rollback. Nêu rủi ro về ELF bounds/overflow, mapping permissions W^X, entry/stack ABI, ownership/lifetime và lỗi user không được làm panic kernel.

Không đóng băng API, ELF subset, relocation types, process arguments layout, startup ABI, libc symbols hoặc acceptance changes trong lượt lập kế hoạch nếu chưa được duyệt. Phân loại baseline đã duyệt, đề xuất, quyết định cần tôi chốt và phần chưa chứng minh.
