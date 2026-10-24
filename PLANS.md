# Kế hoạch hiện hành — M00 bổ sung

Trạng thái milestone: M00=DONE; M01=PLANNED

M00 cũ đã đạt tiêu chí repository/tài liệu. Phần GitHub Public, GPL-3.0-only và Documentation CI được người dùng duyệt triển khai ngày 2026-10-24; đã nghiệm thu local và trên GitHub; chi tiết bằng chứng tại handoff.

## Phạm vi và các bước

- [x] Kiểm tra lại Git, tài liệu, skill, checksum prompt và account; không làm lại M00 cũ.
- [x] Thêm LICENSE chính thức, chuẩn hóa đường dẫn host, quy trình Git/CI/tag và tài liệu liên quan.
- [x] Thêm validator độc lập, unittest và workflow documentation-only; kiểm tra local trước commit.
- [x] Commit bằng danh tính noreply, tạo repository public đúng account, origin và push main.
- [x] Xác minh GitHub metadata/license và CI nghiệm thu trên aa6aeb9ff8ea4620227020ea43e03f430328d6b6.
- [x] Người dùng chính thức xác nhận M00 PASS/DONE; annotated tag milestone/M00 đã push và xác minh đúng commit nghiệm thu.

Không triển khai M01, mã OS, toolchain installation hoặc Release. Tag đã tạo sau xác nhận hoàn thành riêng; commit cập nhật bàn giao sau nghiệm thu không thay đổi đích của tag. Nguồn trình tự là [ROADMAP](ROADMAP.md), cách kiểm tra ở [TESTING](docs/TESTING.md), trạng thái thực tế ở [handoff](docs/SESSION_HANDOFF.md).

## Nghiệm thu và rollback

Local tests đạt, nội dung công khai được review, remote main đúng SHA, repo public/main đúng description, license GPL-3.0-only và Actions thành công trên HEAD nghiệm thu. Tag đã được xác minh ở local và remote. Nếu CI pending/fail hoặc xác thực/quyền bị chặn thì chưa báo phần bổ sung PASS.

Giữ checksum ba prompt và mục tiêu/thứ tự roadmap. Trước public hoàn nguyên riêng diff/file đã kiểm tra; sau public dùng commit mới/revert, không force-push hoặc xóa repository. Không sửa tag công khai. Nếu nghi lộ secret, dừng push và xử lý thu hồi credential an toàn; xóa file trong commit mới không đủ.

## Bước tiếp theo

Thủ tục tag M00 đã hoàn tất. Commit tài liệu đóng phiên phải được push và xác minh CI theo đúng SHA trước báo cáo cuối; không chuyển tag sang commit này. M01 cần lượt chat mới ở Plan Mode và kế hoạch chi tiết riêng.
