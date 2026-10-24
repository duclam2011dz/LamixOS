# Prompt phụ 01 — Chuẩn bị triển khai milestone tiếp theo (Lamix OS)

> **Cách dùng:** Mở workspace Lamix OS trong Codex, **chủ động bật Plan Mode trước khi gửi prompt này**. Thay tất cả placeholder `{{...}}` bằng giá trị thực. Prompt này dùng để **lập kế hoạch và xin duyệt**, không tự triển khai.

## Thông tin milestone

- **Milestone đã hoàn thành gần nhất:** `{{MXX — TÊN_MILESTONE_ĐÃ_HOÀN_THÀNH}}`
- **Milestone tiếp theo cần lập kế hoạch:** `{{MYY — TÊN_MILESTONE_TIẾP_THEO}}`
- **Commit/tag xác nhận (nếu có):** `{{COMMIT_HOẶC_KHÔNG_CÓ}}`
- **Ghi chú riêng của tôi (tùy chọn):** `{{GHI_CHÚ_HOẶC_KHÔNG_CÓ}}`

## Yêu cầu

Bạn là Codex hỗ trợ dự án **Lamix OS**, một hệ điều hành Gaming x86_64 tự phát triển từ đầu. Hãy tuân thủ kiến trúc và các quyết định đã duyệt trong `ROADMAP.md`, `AGENTS.md` và tài liệu dự án. **Toàn bộ báo cáo và tài liệu Markdown viết bằng tiếng Việt**, giữ nguyên tên kỹ thuật/API khi cần.

**Lượt này chỉ làm việc trong Plan Mode. Không chỉnh sửa file, không triển khai code, không tự bắt đầu milestone khác.** Nếu tôi chưa bật Plan Mode, hãy nhắc tôi bật chế độ này; không được coi câu chữ trong prompt là đã chuyển chế độ giao diện.

### 1. Nạp ngữ cảnh có chọn lọc

1. Đọc `AGENTS.md`, `docs/SESSION_HANDOFF.md`, `PLANS.md` (nếu có), và **chỉ phần liên quan** đến hai milestone được nêu trong `ROADMAP.md`.
2. Mở các tài liệu kỹ thuật và skill có liên quan trực tiếp; không đọc hoặc chép lại toàn bộ repo nếu không cần.
3. Kiểm tra trạng thái Git, file thực tế và bằng chứng build/test; xác minh milestone trước có thật sự hoàn thành. Nếu chưa đủ bằng chứng, báo rõ và đề xuất bước xác minh/khắc phục trước khi triển khai milestone mới.
4. Đối chiếu dependencies, quyết định kiến trúc, công cụ WSL2/CMake/Ninja/GCC cross-compiler/QEMU/GDB/CTest/pytest và những hạn chế của môi trường thực tế.

### 2. Lập kế hoạch chi tiết cho milestone tiếp theo

Trình bày bằng tiếng Việt:

- Mục tiêu, kết quả đầu ra, tiêu chí nghiệm thu và những việc **ngoài phạm vi**.
- Dependencies đã thỏa/chưa thỏa, các rủi ro và những giả định cần tôi xác nhận.
- Thiết kế kỹ thuật, ABI/interface/data structures nếu có; đảm bảo không mâu thuẫn với Hybrid Kernel, LBP, boot BIOS/UEFI, driver model và các quyết định đã chốt.
- Chia thành các bước nhỏ có thứ tự, mỗi bước nêu file/module dự kiến tạo hoặc sửa, tác động và phương pháp kiểm chứng.
- Kế hoạch build, unit/integration/regression test: lệnh dự kiến, QEMU profile BIOS/UEFI tương ứng, serial log markers, kết quả mong đợi, timeout và cách debug bằng GDB khi cần.
- Kế hoạch rollback/recovery nếu thất bại; không thao tác lên disk thật của host.
- Tài liệu cần cập nhật **sau khi triển khai thành công**: `PLANS.md`, `CHANGELOG.md`, `docs/SESSION_HANDOFF.md`, phần trạng thái tương ứng trong `ROADMAP.md` và các tài liệu kỹ thuật liên quan.
- Những quyết định cần tôi phê duyệt trước khi code.

### 3. Quy tắc phê duyệt

**DỪNG sau khi trình bày kế hoạch.** Chờ tôi phản hồi và duyệt; không tự triển khai, không đánh dấu milestone là hoàn thành. Sau khi tôi duyệt và chuyển sang chế độ triển khai, chỉ làm đúng phạm vi đã duyệt. Nếu cần thay đổi kiến trúc, phạm vi hoặc tiêu chí nghiệm thu, phải quay lại lập kế hoạch và xin phép.

### 4. Tiết kiệm usage limit

Ưu tiên đọc theo đường dẫn, heading, symbol và diff; không in log dài khi không cần; không tạo tài liệu trùng lặp. Phân biệt rõ `PASS`, `FAIL`, `SKIPPED`, `NOT RUN` và không bịa kết quả kiểm thử.

**Đầu ra ngay bây giờ:** báo cáo xác minh milestone trước + kế hoạch chi tiết milestone tiếp theo + câu hỏi cần chốt; **chờ phê duyệt**.
