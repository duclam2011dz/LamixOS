# Đóng góp

## Quy trình

1. Đọc [AGENTS.md](AGENTS.md), [handoff](docs/SESSION_HANDOFF.md), milestone liên quan trong [roadmap](ROADMAP.md) và [kế hoạch](PLANS.md); kiểm tra workspace/Git thực tế.
2. Với milestone mới, bật Plan Mode thật, chốt mục tiêu, ngoài phạm vi, phụ thuộc, interfaces, bước nhỏ, test, rủi ro và rollback; chờ người dùng duyệt.
3. Sau khi chuyển chế độ triển khai, chỉ làm phạm vi đã duyệt. Dừng phần phụ thuộc nếu cần đổi kiến trúc/phạm vi và xin duyệt kế hoạch mới.
4. Review diff, chạy kiểm tra phù hợp, báo kết quả thật và giới hạn. Kiểm tra cả file mới chưa tracked.
5. Cập nhật kế hoạch, changelog, handoff và trạng thái roadmap có bằng chứng. Không tự bắt đầu milestone kế tiếp.

## Review và kiểm thử

Theo [quy ước](docs/CONVENTIONS.md), [quy tắc vận hành](docs/RULES.md) và [kiểm thử](docs/TESTING.md). Review đặc biệt quyền truy cập, ownership, freestanding/host và ABI khi chúng thực sự xuất hiện. Không thêm API hay quyết định ABI chỉ để làm đầy tài liệu.

M00 chưa yêu cầu build OS. Commit/push chỉ trong phạm vi được yêu cầu; milestone tag cần xác nhận hoàn thành riêng. Giữ tài liệu/mã hiện có của người dùng; tránh thao tác hoàn nguyên hàng loạt.

## Git và CI

Dự án dùng [GPL-3.0-only](LICENSE); đóng góp phải có quyền phân phối theo giấy phép này. Xem xét tương thích license trước khi thêm thành phần bên thứ ba; không tự thêm exceptions hoặc quyền “or later”.

Tạo branch cho thay đổi và PR hướng tới main. Commit theo `docs(m00): mô tả`, `ci(m00): mô tả`, `chore(m00): mô tả`, thay mã milestone đúng công việc. Không đưa secrets hoặc dữ liệu cá nhân không cần thiết vào commit; dùng GitHub noreply nếu muốn tránh công khai email cá nhân. Cấu hình danh tính chỉ ở repository khi được yêu cầu.

Chạy hai lệnh kiểm tra ở [TESTING](docs/TESTING.md), review diff/staged và CI của PR trước merge. Không dùng force-push lên main, không tự sửa lịch sử công khai. Remote chính thức là origin; kiểm tra URL trước push. Chưa cấu hình branch protection; CI gate hiện là quy trình đóng góp.

Annotated tags `milestone/M00`…`milestone/M44` chỉ tạo sau nghiệm thu và xác nhận của người dùng, trỏ SHA đã CI xanh. Không di chuyển/xóa/ghi đè tag công khai. Version tags và Releases độc lập với milestone, chỉ tạo khi được duyệt riêng.
