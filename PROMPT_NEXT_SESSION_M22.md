# Prompt chuyển phiên Codex — Lập kế hoạch M22

> Gửi prompt này trong một phiên Codex mới. **Trước khi khảo sát milestone, hãy chủ động bật Codex Plan Mode thật trên giao diện.** Nội dung prompt và `AGENTS.md` không thể tự chuyển chế độ UI.

Bạn là Codex tiếp tục phát triển **Lamix OS**. Làm việc và viết Markdown bằng tiếng Việt rõ ràng; giữ nguyên tên kỹ thuật, API, symbol, đường dẫn và lệnh khi cần.

## Nhiệm vụ

Chỉ lập kế hoạch cho **M22 — Handle-based capabilities và IPC** theo `ROADMAP.md`. M21 đã `DONE`; M22 vẫn `PLANNED` và chưa được duyệt triển khai. Không bắt đầu code M22, M23 hay milestone khác trong lượt này.

## Plan Mode và giới hạn

- Nếu chưa ở Codex Plan Mode thật, chỉ nhắc tôi chủ động bật Plan Mode rồi dừng; không khảo sát milestone trước đó.
- Sau khi ở Plan Mode, chỉ đọc, xác minh và lập kế hoạch. Không sửa/tạo file dự án, triển khai code/API/tests/fixtures, cài dependencies, chạy build/CTest/QEMU/CI/benchmark, commit/push/tag, thay đổi GitHub hoặc milestone status.
- Kết thúc bằng kế hoạch M22, các quyết định còn mở cần tôi chốt và dừng chờ tôi duyệt rõ ràng. Việc duyệt M22 phải diễn ra trước khi chuyển sang chế độ triển khai.

## Trạng thái bàn giao đã xác minh

- Repository: `https://github.com/duclam2011dz/LamixOS`; workspace: `/home/lam/projects/LamixOS`; nhánh chính `main`; license `GPL-3.0-only`.
- M21 acceptance commit: `6793d8f0a622fcd9f4100921e7932fcb46414540`; PR [#22](https://github.com/duclam2011dz/LamixOS/pull/22), merge commit `20073de921c8a13572c57ca9f106ed83cdd4f886`.
- Annotated tag `milestone/M21`, tag object `f52683213c48c8851f31e8aef653462d204d38ad`, trỏ acceptance commit M21.
- Bốn workflow SUCCESS trên acceptance SHA: Toolchain [41270742769](https://github.com/duclam2011dz/LamixOS/actions/runs/41270742769), Harness [41270742714](https://github.com/duclam2011dz/LamixOS/actions/runs/41270742714), Documentation [41270742671](https://github.com/duclam2011dz/LamixOS/actions/runs/41270742671), Boot Contract [41270742711](https://github.com/duclam2011dz/LamixOS/actions/runs/41270742711).
- M21 local CTest **3/3 PASS**; guest SeaBIOS `pc` và OVMF `q35` **3/3 mỗi firmware PASS**; M17/M19/M20 regressions, docs validator và unittest tài liệu **PASS**. PC thật và firmware ngoài profile QEMU **NOT RUN**; audit bảo mật độc lập **NOT RUN**.
- M18 vẫn `PLANNED`/để dành. M23+ ngoài phạm vi. `scripts/lamix_qemu.py --profile latest` vẫn dùng profile M20; M21 chọn riêng bằng `--profile m21`.

Snapshot này chỉ là gợi ý. Hãy xác minh lại `git status`, branch/HEAD, remote, PR/tag và CI nếu các dịch vụ khả dụng; nếu không xác minh được, ghi `NOT RUN` hoặc `chưa xác minh`.

## Quy tắc và tài liệu cần đọc

1. Đọc `AGENTS.md`, các `AGENTS.md` áp dụng, `docs/SESSION_HANDOFF.md`, `PLANS.md` và mục M22 trong `ROADMAP.md`.
2. Đọc `CONTRIBUTING.md`, `docs/CONVENTIONS.md`, `docs/RULES.md`, `docs/DECISIONS.md`, `docs/SECURITY.md`, `docs/ARCHITECTURE.md`, `docs/API.md`, `docs/MEMORY_MANAGEMENT.md`, `docs/TESTING.md` và tài liệu kỹ thuật khác có liên quan trực tiếp.
3. Kiểm tra code M20/M21 và các interface liên quan tới process, scheduler, memory mapping, ownership/lifetime và syscall boundary. Xác định bằng chứng hiện có; không giả định capability handle, IPC hay service API đã tồn tại.
4. `ROADMAP.md` có thẩm quyền về mục tiêu và thứ tự. M22 theo roadmap gồm object handles có rights, duplication/transfer, channels/messages, shared memory khi an toàn và quyền tối thiểu. Acceptance hiện hành: negative permission tests, race/close/reuse tests. Không tự mở rộng hoặc đổi các tiêu chí đó.

## Đầu ra kế hoạch

Đối chiếu các lựa chọn thiết kế với code hiện tại: object/handle ownership và lifetime, rights model và validation, close/duplicate/transfer semantics, channel/message behavior, shared-memory mapping/cleanup khi an toàn, syscall/user boundary, concurrency và race handling. Phân biệt baseline đã nghiệm thu, thiết kế đề xuất, quyết định cần tôi chốt, rủi ro/chưa chứng minh. Đưa ra các bước triển khai theo thứ tự với file/module dự kiến, test host và guest phù hợp, regression M19/M20/M21, acceptance đo được và rollback.

Không đóng băng API, bit rights, handle encoding, syscall numbers, channel semantics, shared-memory contract hay test expectation mới trong lượt lập kế hoạch nếu chúng chưa được duyệt. Chỉ trình bày đề xuất và chờ tôi chốt.
