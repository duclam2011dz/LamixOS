# Bảo mật

**Chưa triển khai.** Baseline: [roadmap, mục 1](../ROADMAP.md); triển khai theo M20–M22 và các milestone hardening liên quan. Chưa có bảo đảm bảo mật hoặc kết quả audit.

Định hướng là handle-based capabilities, process isolation, quyền tối thiểu, validation user pointers/handles và NX/W^X khi khả dụng. Driver kernel-mode và user-mode đều tồn tại; boundary và quyền cần được kiểm chứng khi có interfaces.

Disk/image phải bảo vệ GPT metadata và User Data. LamFS cần bounds checks, metadata consistency và kiểm thử dữ liệu hỏng; A/B updates cần recovery có bằng chứng. Chính sách cụ thể chưa đặc tả, không tuyên bố đã an toàn.

Lỗi hoặc nguy cơ mới được ghi ở [handoff](SESSION_HANDOFF.md), cùng tác động và bước xác minh; thay đổi baseline phải qua approval gate.
