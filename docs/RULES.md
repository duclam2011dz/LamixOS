# Quy tắc vận hành

Quy trình phê duyệt có nguồn tại [AGENTS.md](../AGENTS.md). Đây là quy tắc thao tác bổ sung.

- Chỉ làm milestone/sub-milestone đã duyệt; không tự bắt đầu mốc kế tiếp.
- Không tự ghi disk thật, format partition host hoặc xóa tài liệu/mã chưa duyệt. Chỉ dùng image QEMU/test riêng khi đến milestone tương ứng; xác nhận đường dẫn trước khi xóa.
- Không đổi quyền hoặc thay thế thư mục được bảo vệ để lách giới hạn môi trường. Ghi blocker và tiếp tục phần độc lập được phép.
- Rollback theo diff và danh sách file đã tạo; giữ chỉnh sửa của người dùng. Không dùng `git reset --hard` hoặc `git clean` để hoàn nguyên hàng loạt.
- Test hẹp lúc lặp, regression phù hợp khi nghiệm thu; không chạy test nặng không có mục đích.
- Chỉ báo DONE khi đủ tiêu chí và bằng chứng theo [TESTING](TESTING.md). Không suy ra bảo mật/ổn định từ compile thành công.
