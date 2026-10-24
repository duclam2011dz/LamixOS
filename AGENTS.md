# Hướng dẫn Codex — Lamix OS

Làm việc và viết Markdown bằng tiếng Việt; giữ nguyên tên API, symbol và đường dẫn khi cần.

## Bắt đầu phiên

Đọc tài liệu này, [handoff](docs/SESSION_HANDOFF.md), đúng milestone trong [roadmap](ROADMAP.md) và [kế hoạch hiện hành](PLANS.md). Kiểm tra file thực tế và `git status`; báo rõ nếu Git chưa dùng được. Chỉ mở thêm tài liệu liên quan.

## Plan-first và phạm vi

Trước mỗi milestone mới, người dùng phải bật Plan Mode thật. Nếu chưa bật, nhắc chuyển chế độ; AGENTS.md không tự đổi chế độ UI. Khảo sát, lập kế hoạch và dừng chờ duyệt rõ ràng. Chỉ triển khai phạm vi đã duyệt khi chuyển sang chế độ triển khai. Duyệt một milestone không cho phép làm milestone tiếp theo. Thay đổi kiến trúc hoặc mở rộng phạm vi phải quay lại lập kế hoạch xin duyệt.

[ROADMAP.md](ROADMAP.md) là nguồn thẩm quyền về mục tiêu và thứ tự. Chỉ sửa trạng thái/ghi chú nghiệm thu có bằng chứng; thay đổi nội dung định hướng phải được duyệt riêng.

## Tìm đúng tài liệu

- Quy trình đóng góp: [CONTRIBUTING.md](CONTRIBUTING.md); quy ước: [CONVENTIONS](docs/CONVENTIONS.md).
- Quy tắc thao tác: [RULES](docs/RULES.md); bằng chứng: [TESTING](docs/TESTING.md).
- Kiến trúc: [ARCHITECTURE](docs/ARCHITECTURE.md); lý do quyết định: [DECISIONS](docs/DECISIONS.md).
- Khám phá skill qua `.agents/skills/*/SKILL.md`: đọc metadata rồi chỉ tải skill phù hợp. [docs-consistency](.agents/skills/docs-consistency/SKILL.md) dành cho rà soát tài liệu. Skill không mở rộng quyền hoặc phạm vi.

Cuối lượt triển khai có thay đổi, cập nhật kế hoạch, changelog, handoff ngắn và trạng thái roadmap đúng bằng chứng. Báo PASS/FAIL/SKIPPED/NOT RUN; không suy ra hoàn thành từ compile. Không tự chạy M01 hoặc ghi disk thật.

## Git, CI và nghiệm thu

Dùng origin cho repository chính thức, main là nhánh mặc định. Commit theo dạng `docs(m00): mô tả`, `ci(m00): mô tả`, `chore(m00): mô tả`; đổi mã milestone khi cần. Review nội dung staged, secrets và dữ liệu cá nhân trước khi push. Không force-push main hoặc sửa lịch sử/tag công khai khi chưa được duyệt.

Chạy validator và unittest theo [TESTING](docs/TESTING.md) trước commit. CI phải thành công trên commit nghiệm thu; phân biệt local PASS với GitHub Actions PENDING/FAIL. PR cần review và CI xanh trước merge; đây là quy trình, không giả định branch protection đã được bật.

Chỉ tạo annotated tag `milestone/MXX` khi đủ tiêu chí và người dùng xác nhận hoàn thành. Tag trỏ commit đã nghiệm thu, không di chuyển/xóa/ghi đè tag đã push. Version tags và GitHub Releases cần duyệt riêng, không tự phát hành mỗi milestone.
