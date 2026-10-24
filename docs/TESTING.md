# Kiểm thử và bằng chứng

## Local validation

Chạy từ root bằng Python 3.10 trở lên; CI dùng Python 3.12. Không cài dependency và không cần mạng:

```bash
python3 scripts/validate_docs.py
python3 -m unittest discover -s tests -p 'test_validate_docs.py'
git -c core.whitespace=-blank-at-eol diff --check
git -c core.whitespace=-blank-at-eol diff --cached --check
```

[Validator](../scripts/validate_docs.py) kiểm tra cả Markdown chưa tracked: đủ cấu trúc, UTF-8, newline cuối file, whitespace, link/anchor nội bộ, metadata skill và trạng thái milestone. Chỉ cho phép đúng hai spaces cuối dòng làm Markdown hard break. Lệnh Git trên tắt kiểm tra blank-at-eol riêng cho lần chạy vì Git mặc định cũng báo lỗi hard break hợp lệ; validator vẫn kiểm tra whitespace chặt theo quy tắc hai spaces. Không lint văn phong hoặc suy luận tính đúng kỹ thuật.

Hỗ trợ Markdown của dự án: inline links/images, reference links full/collapsed/shortcut với definitions một dòng, destinations có ngoặc cân bằng hoặc angle brackets, URL encoding; ATX/setext headings, Unicode và suffix heading trùng. Bỏ fenced/indented code và inline code khi kiểm tra link; không truy cập URL ngoài. HTML tùy ý hoặc toàn bộ CommonMark không phải phạm vi parser; mở rộng cùng tests khi dự án dùng cú pháp mới.

Ba file ROADMAP, PLANS và SESSION_HANDOFF có đúng một dòng máy đọc được, ví dụ sau chỉ minh họa cú pháp:

```text
Trạng thái milestone: M00=VERIFYING; M01=PLANNED
```

Giá trị dùng tập trạng thái trong roadmap. Mục milestone không ghi trạng thái riêng mặc định PLANNED; dòng chuẩn phải khớp M00/M01 và giữa ba file. Validator kiểm tra M00–M44 đủ, đúng thứ tự, không trùng; các quy tắc kiến trúc, changelog thật và chất lượng handoff vẫn cần review theo [skill](../.agents/skills/docs-consistency/SKILL.md).

[Unittest](../tests/test_validate_docs.py) dùng repository fixtures tạm cho positive/negative tests; không sửa tài liệu thật. Validator không trực tiếp chạy skill Codex. Kiểm tra metadata đơn giản trong Python không thay thế review hành vi skill.

Trước public, review danh sách staged, secrets, binary/artifacts, đường dẫn host và email commit. Dò mẫu credentials chỉ hỗ trợ phát hiện, không bảo đảm tuyệt đối. Giữ nguyên checksum ba prompt gốc; roadmap chỉ đổi trạng thái/ghi chú được duyệt. Git diff không bao phủ untracked, nên luôn chạy validator và review danh sách file mới.

## GitHub Actions và repository

Workflow [Documentation CI](../.github/workflows/docs-ci.yml), job Validate documentation: push main, PR tới main và workflow_dispatch. Runner ubuntu-24.04, Python 3.12, timeout 5 phút, contents: read; actions chính thức ghim full SHA, checkout không lưu credentials. Không có secrets riêng hoặc job OS.

Xác minh run theo đúng SHA, lưu URL và kết luận. Kiểm tra API repository cho visibility/default branch/license và đối chiếu SHA remote main. Trạng thái pending hoặc workflow mới tồn tại không phải PASS. Local tests không chứng minh workflow đã chạy trên GitHub. Trigger PR chưa có PR thật thì ghi NOT RUN.

Tag milestone chỉ tạo sau xác nhận hoàn thành của người dùng và CI xanh. Không tạo Release từ milestone tags.

## Mở rộng sau M00

**Chưa triển khai.** M01 thêm toolchain/build checks khi có target thật; M02 thêm harness. Khi có bootable image, thêm BIOS/SeaBIOS và UEFI/OVMF smoke tests, timeout, panic detection và serial assertions. Upload logs khi lỗi; thêm subsystem regression khi có subsystem tương ứng theo [roadmap](../ROADMAP.md).

Release tương lai chỉ chạy từ version tag đã duyệt, xác minh trước phát hành và đính kèm artifacts/checksums khi tồn tại. Chỉ job release cần thiết mới có quyền ghi; CI mặc định giữ read-only. Chưa tạo workflow release hoặc các job chờ giả.

## Ý nghĩa kết quả

- PASS: kiểm tra thực sự chạy và đạt; ghi lệnh, môi trường, exit status/assertions và bằng chứng.
- FAIL: đã chạy nhưng không đạt; ghi ảnh hưởng.
- PENDING: run đang chờ/đang chạy, chưa có kết luận.
- SKIPPED: trường hợp bỏ qua có lý do.
- NOT RUN: chưa chạy; không suy ra kết quả.

Build/boot/QEMU trong M00: NOT RUN, chưa có mã hoặc harness. Không bịa FPS, uptime, logs hoặc bảo đảm bảo mật.
