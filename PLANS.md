# Kế hoạch hiện hành — M00 bổ sung

Trạng thái milestone: M00=VERIFYING; M01=PLANNED

M00 cũ đã đạt tiêu chí repository/tài liệu. Phần GitHub Public, GPL-3.0-only và Documentation CI được người dùng duyệt triển khai ngày 2026-10-24; hiện chờ nghiệm thu trên GitHub.

## Phạm vi và các bước

- [x] Kiểm tra lại Git, tài liệu, skill, checksum prompt và account; không làm lại M00 cũ.
- [x] Thêm LICENSE chính thức, chuẩn hóa đường dẫn host, quy trình Git/CI/tag và tài liệu liên quan.
- [x] Thêm validator độc lập, unittest và workflow documentation-only; kiểm tra local trước commit.
- [ ] Commit bằng danh tính noreply, tạo repository public đúng account, origin và push main.
- [ ] Xác minh GitHub metadata/license và CI trên HEAD cuối; ghi bằng chứng thật.
- [ ] Người dùng xác nhận hoàn thành rồi mới tạo/push annotated tag milestone/M00.

Không triển khai M01, mã OS, toolchain installation hoặc Release. Tag chờ xác nhận là bước riêng sau nghiệm thu, không tự tạo chỉ vì kế hoạch được duyệt. Nguồn trình tự là [ROADMAP](ROADMAP.md), cách kiểm tra ở [TESTING](docs/TESTING.md), trạng thái thực tế ở [handoff](docs/SESSION_HANDOFF.md).

## Nghiệm thu và rollback

Local tests đạt, nội dung công khai được review, remote main đúng SHA, repo public/main đúng description, license GPL-3.0-only và Actions thành công trên HEAD nghiệm thu. Báo riêng tag chờ xác nhận. Nếu CI pending/fail hoặc xác thực/quyền bị chặn thì chưa báo phần bổ sung PASS.

Giữ checksum ba prompt và mục tiêu/thứ tự roadmap. Trước public hoàn nguyên riêng diff/file đã kiểm tra; sau public dùng commit mới/revert, không force-push hoặc xóa repository. Không sửa tag công khai. Nếu nghi lộ secret, dừng push và xử lý thu hồi credential an toàn; xóa file trong commit mới không đủ.

## Bước tiếp theo

Hoàn tất các gate GitHub và ghi SHA/run URLs. Sau nghiệm thu, chờ xác nhận riêng cho milestone/M00; M01 cần lượt Plan Mode và kế hoạch chi tiết riêng.
