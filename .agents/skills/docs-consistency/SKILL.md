---
name: docs-consistency
description: Rà soát tính nhất quán tài liệu milestone Lamix OS, trạng thái và bằng chứng bàn giao; không triển khai subsystem OS.
---
# Kiểm tra tính nhất quán tài liệu

Dùng khi review hoặc nghiệm thu tài liệu milestone. Chạy tại root workspace Lamix OS; đọc AGENTS.md, docs/SESSION_HANDOFF.md, PLANS.md và đúng milestone trong ROADMAP.md. Chỉ mở tài liệu liên quan đến thay đổi.

1. Đối chiếu git status/diff với file thực tế, bao gồm file untracked. Nếu Git không dùng được, ghi blocker và dùng danh sách file/checksum; không báo kiểm tra Git PASS.
2. Giữ ROADMAP.md làm nguồn mục tiêu và thứ tự. Chỉ thay trạng thái/ghi chú có bằng chứng trong phạm vi được duyệt; báo khác biệt thay vì tự sửa baseline.
3. Kiểm tra trách nhiệm: PLANS là kế hoạch hiện hành, DECISIONS ghi lý do có nguồn, CHANGELOG ghi thay đổi thật, SESSION_HANDOFF là trạng thái ngắn để tiếp tục. Loại nội dung trùng bằng liên kết đến nguồn.
4. Kiểm tra UTF-8, đủ file theo kế hoạch, link tương đối/anchor, whitespace và placeholders. Kiểm tra cả file mới; git diff --check riêng lẻ không bao phủ untracked. Không truy cập mạng chỉ để kiểm tra link nội bộ.
5. Đối chiếu các tuyên bố triển khai với code/test/logs. Giữ nhãn Chưa triển khai khi chưa có subsystem; không suy ra ABI, hiệu năng hoặc bảo mật từ định hướng.
6. Báo PASS/FAIL/SKIPPED/NOT RUN cùng lệnh, bằng chứng, blocker và bước tiếp theo. Đồng bộ trạng thái kế hoạch/roadmap/handoff theo kết quả, không tự mở milestone tiếp theo.

Chạy `python3 scripts/validate_docs.py` và `python3 -m unittest discover -s tests -p 'test_validate_docs.py'` từ root theo docs/TESTING.md. Validator chạy độc lập trên local/CI; GitHub Actions không trực tiếp thực thi skill Codex. Phân biệt local PASS với run GitHub theo đúng SHA; không tạo milestone tag trước xác nhận hoàn thành riêng của người dùng. Theo docs/TESTING.md để chọn kiểm tra phù hợp. Chỉ sửa tài liệu khi phạm vi hiện tại cho phép; skill không thay approval gate và không cho phép vượt giới hạn quyền.
