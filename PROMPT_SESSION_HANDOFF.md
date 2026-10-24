# Prompt phụ 02 — Bàn giao khi chuyển phiên Codex (Lamix OS)

> **Cách dùng:** Gửi prompt này **ở cuối phiên Codex cũ**, sau khi hoàn tất hoặc tạm dừng công việc. Thay placeholder `{{...}}`. Đây là tác vụ **bàn giao tài liệu, không phải triển khai milestone tiếp theo**. Nếu phiên cũ đang trong Plan Mode/chỉ đọc, yêu cầu Codex xuất bản nháp handoff để bạn duyệt hoặc chuyển sang chế độ cho phép chỉnh sửa trước khi ghi file.

## Mốc bàn giao

- **Milestone hoàn thành gần nhất (đã có bằng chứng):** `{{MXX — TÊN_MILESTONE_ĐÃ_HOÀN_THÀNH}}`
- **Milestone sẽ xử lý trong phiên mới:** `{{MYY — TÊN_MILESTONE_TIẾP_THEO}}`
- **Trạng thái hiện tại:** `{{HOÀN_THÀNH_HOẶC_ĐANG_DỞ}}`
- **Commit/tag gần nhất (nếu có):** `{{COMMIT_HOẶC_KHÔNG_CÓ}}`

## Yêu cầu bàn giao

Bạn là Codex đang kết thúc một phiên làm việc trên **Lamix OS**. Hãy chuẩn bị để **một phiên Codex mới tiếp tục mà không cần tôi kể lại toàn bộ lịch sử**. Toàn bộ nội dung Markdown và báo cáo phải bằng **tiếng Việt dễ hiểu**.

1. Đối chiếu `git status`, diff, code, `ROADMAP.md`, `PLANS.md`, `CHANGELOG.md`, tài liệu và logs có sẵn. **Không suy đoán một việc đã hoàn thành nếu chưa có bằng chứng.** Nếu milestone đang dở, ghi chính xác phần đã xong/chưa xong và không đổi thành `DONE`.
2. Cập nhật **`docs/SESSION_HANDOFF.md`** thành bản tóm tắt mới, ngắn (khoảng 1–2 trang), gồm:
   - Ngày/phiên bàn giao và trạng thái repo, branch/commit (nếu có).
   - Milestone đã hoàn thành gần nhất, bằng chứng nghiệm thu; milestone kế tiếp theo `ROADMAP.md`.
   - Các file/module đã tạo/sửa, tác động chính; thay đổi chưa commit hoặc đang dở.
   - Lệnh build/test thực sự đã chạy, môi trường/QEMU profile, kết quả `PASS`/`FAIL`/`SKIPPED`/`NOT RUN`, vị trí log nếu có.
   - Bugs/blockers, giới hạn đã biết, rủi ro, quyết định kiến trúc đã duyệt hoặc còn chờ duyệt.
   - **Bước tiếp theo chính xác** cho phiên mới, nhưng **không tự thực hiện bước đó**.
3. Cập nhật `CHANGELOG.md` **chỉ với thay đổi thực sự đã hoàn thành**; cập nhật trạng thái milestone trong `ROADMAP.md` **chỉ khi đủ bằng chứng**, không viết lại mục tiêu hoặc thứ tự roadmap. Đồng bộ `PLANS.md` và các tài liệu kỹ thuật chỉ khi nội dung thật sự thay đổi. Nếu không được phép chỉnh sửa, đưa ra bản nháp/diff thay vì giả vờ đã lưu.
4. Không tạo nhiều bản handoff chồng chất, không sao chép toàn bộ roadmap, không nhét transcript dài vào file. `AGENTS.md` là quy tắc lâu dài; `SESSION_HANDOFF.md` là trạng thái ngắn hạn. Giữ mọi đường dẫn và lệnh đủ cụ thể để phiên mới kiểm chứng.
5. Không triển khai tính năng mới, không tự chuyển sang milestone tiếp theo, không chạy lại test nặng nếu không cần để xác minh trạng thái; không xóa file/log/commit của người dùng.

## Đầu ra cuối phiên

- Nêu rõ những file tài liệu đã cập nhật (hoặc chưa cập nhật và lý do).
- Tóm tắt trạng thái thực tế và vấn đề còn tồn tại.
- Đưa ra **một đoạn mở phiên mới thật ngắn**, trong đó yêu cầu Codex đọc `AGENTS.md`, `docs/SESSION_HANDOFF.md`, `PLANS.md` và đúng milestone trong `ROADMAP.md`, kiểm tra repo thực tế, rồi **vào Plan Mode trước khi lập kế hoạch cho `{{MYY — TÊN_MILESTONE_TIẾP_THEO}}`**.
- Nhắc tôi **chủ động bật Plan Mode trong phiên mới**; không tuyên bố có thể bật Plan Mode chỉ bằng nội dung `AGENTS.md`.

**Chỉ bàn giao; không triển khai milestone mới.**
