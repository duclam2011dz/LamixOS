# Quyết định kiến trúc — ADR

## Baseline được cung cấp

Nguồn: [ROADMAP.md bản 1.0](../ROADMAP.md) và [master prompt](../MASTER_PROMPT_LAMIX_OS.md). Người dùng đã chốt baseline trước M00. Các tài liệu đó ghi lựa chọn kiến trúc; không có lịch sử thảo luận đầy đủ để suy đoán lý do hoặc ngày quyết định riêng lẻ.

M00 không thêm quyết định ABI/OS. Lập ADR mới khi có lựa chọn thực tế được duyệt; không sao chép toàn bảng baseline vào đây.

## Mẫu cho ADR tương lai

Mỗi ADR cần mã và tiêu đề, trạng thái (đề xuất/đã duyệt/được thay thế), bối cảnh và nguồn dữ kiện, phương án cân nhắc, quyết định được duyệt, lý do, hệ quả/rủi ro, bằng chứng và liên kết kế hoạch liên quan. Ghi ngày duyệt thực tế và ADR thay thế nếu có; chưa biết thì nói rõ chưa xác minh.

## ADR-001 — Xuất bản và hạ tầng M00

**Trạng thái:** đã duyệt ngày 2026-10-24 qua kế hoạch M00 bổ sung.

- Repository public LamixOS trên account cá nhân đã xác thực duclam2011dz, default branch main, remote origin; không tạo organization hoặc GitHub Release trong M00.
- GPL-3.0-only theo lựa chọn người dùng; LICENSE giữ nguyên tiếng Anh chính thức. Không chọn “or later”.
- Commit theo milestone, dùng email noreply local. Annotated milestone tags chỉ sau xác nhận hoàn thành; version tags dành cho phát hành được duyệt riêng. Không sửa lịch sử/tag đã công khai.
- CI M00 chỉ kiểm tra tài liệu và validator; Python standard library để chạy lại trên WSL2/runner mà không phụ thuộc Codex. Chưa có target OS nên chưa chạy build/boot.

Hệ quả: dự án công khai phải review nội dung trước push; CI chỉ chứng minh các kiểm tra đã chạy. Pin actions bằng SHA, permissions contents: read, không lưu credential checkout. PR cần review và CI xanh theo quy trình; chưa bật branch protection. Kết quả xuất bản thực tế ở [handoff](SESSION_HANDOFF.md), không suy ra thành công từ quyết định này.
