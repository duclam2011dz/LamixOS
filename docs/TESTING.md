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
Trạng thái milestone: M00=VERIFYING; M01=PLANNED; M02=PLANNED
```

Giá trị dùng tập trạng thái trong roadmap. Mục milestone không ghi trạng thái riêng mặc định PLANNED; dòng chuẩn phải khớp M00/M01/M02 và giữa ba file. Validator kiểm tra M00–M44 đủ, đúng thứ tự, không trùng; các quy tắc kiến trúc, changelog thật và chất lượng handoff vẫn cần review theo [skill](../.agents/skills/docs-consistency/SKILL.md).

[Unittest](../tests/test_validate_docs.py) dùng repository fixtures tạm cho positive/negative tests; không sửa tài liệu thật. Validator không trực tiếp chạy skill Codex. Kiểm tra metadata đơn giản trong Python không thay thế review hành vi skill.

Trước public, review danh sách staged, secrets, binary/artifacts, đường dẫn host và email commit. Dò mẫu credentials chỉ hỗ trợ phát hiện, không bảo đảm tuyệt đối. Giữ nguyên checksum ba prompt gốc; roadmap chỉ đổi trạng thái/ghi chú được duyệt. Git diff không bao phủ untracked, nên luôn chạy validator và review danh sách file mới.

## GitHub Actions và repository

Workflow [Documentation CI](../.github/workflows/docs-ci.yml), job Validate documentation: push main, PR tới main và workflow_dispatch. Runner ubuntu-24.04, Python 3.12, timeout 5 phút, contents: read; actions chính thức ghim full SHA, checkout không lưu credentials. Không có secrets riêng hoặc job OS.

Xác minh run theo đúng SHA, lưu URL và kết luận. Kiểm tra API repository cho visibility/default branch/license và đối chiếu SHA remote main. Trạng thái pending hoặc workflow mới tồn tại không phải PASS. Local tests không chứng minh workflow đã chạy trên GitHub. Trigger PR chưa có PR thật thì ghi NOT RUN.

Tag milestone chỉ tạo sau xác nhận hoàn thành của người dùng và CI xanh. Không tạo Release từ milestone tags.

## Mở rộng sau M00

**Kiểm thử OS chưa triển khai.** M01 có toolchain/build sample checks theo phần dưới; M02 có harness, launch-only và boot skeleton SKIPPED theo phần M02. Khi có bootable image, thêm BIOS/SeaBIOS và UEFI/OVMF smoke tests, timeout, panic detection và serial assertions. Upload logs khi lỗi; thêm subsystem regression khi có subsystem tương ứng theo [roadmap](../ROADMAP.md).

Release tương lai chỉ chạy từ version tag đã duyệt, xác minh trước phát hành và đính kèm artifacts/checksums khi tồn tại. Chỉ job release cần thiết mới có quyền ghi; CI mặc định giữ read-only. Chưa tạo workflow release hoặc các job chờ giả.

## Ý nghĩa kết quả

- PASS: kiểm tra thực sự chạy và đạt; ghi lệnh, môi trường, exit status/assertions và bằng chứng.
- FAIL: đã chạy nhưng không đạt; ghi ảnh hưởng.
- PENDING: run đang chờ/đang chạy, chưa có kết luận.
- SKIPPED: trường hợp bỏ qua có lý do.
- NOT RUN: chưa chạy; không suy ra kết quả.

Build/boot/QEMU trong M00: NOT RUN, chưa có mã hoặc harness. Không bịa FPS, uptime, logs hoặc bảo đảm bảo mật.

## Kiểm thử M01

Theo [TOOLCHAIN](TOOLCHAIN.md), kích hoạt .venv rồi chạy:

```bash
python scripts/doctor.py --profile wsl2 --json out/m01/doctor.json
python -m pytest -q tests
python scripts/smoke.py --reproducible
```

CTest trong mỗi profile gọi host tests và verifier ELF, không thực thi target ELF. Smoke runner build hai profiles và so hashes của hai source roots tạm theo từng profile; JSON/logs trong out/m01. Negative tests dùng mocked commands, ELF fixtures, configure thật với prefix thiếu/target host và archive/prefix guards. Không coi missing cross compiler hoặc không có test là PASS.

[Toolchain CI](../.github/workflows/toolchain-ci.yml) bootstrap GNU source theo manifest có hash, doctor profile ci, build/CTest/reproducibility và validation tài liệu. Cache khớp chính xác; logs lỗi được upload, không upload compiler. Runner ubuntu-22.04, Python 3.12, timeout 60 phút, contents: read. Documentation CI vẫn độc lập. Chỉ báo M01 PASS khi cả hai workflows thành công trên SHA nghiệm thu.

Boot BIOS/UEFI, kernel và QEMU smoke: NOT RUN, ngoài M01; chưa bật boot job. Việc firmware đọc được và QEMU --version chạy được không chứng minh OS boot.

## QEMU harness M02

[Runner](../scripts/qemu_harness.py) và [CLI](../scripts/run_qemu.py) dùng Python standard library. Pytest chỉ cần cho tests; Harness CI riêng không bootstrap cross-compiler. Kích hoạt .venv và chạy từ root:

```bash
python -m pytest -q tests/qemu/test_harness.py
python -m pytest -q tests/qemu/test_launch.py
cmake -S tests/qemu -B build/m02 -DPython3_EXECUTABLE="$PWD/.venv/bin/python"
ctest --test-dir build/m02 --no-tests=error --output-on-failure --output-junit "$PWD/out/m02/ctest.xml"
```

CTest host-only (LANGUAGES NONE) chạy fixture suite, emulator-launch pytest (BIOS/UEFI) và hai boot skeleton. Firmware/QEMU thiếu là FAIL của launch tests, không tự SKIP. `pytest.ini` đăng ký marker qemu; host-tests trong M01 dùng `-m "not qemu"` để không lặp launch thật theo Debug/Release. Fixture tests vẫn là host regression.

### Ý nghĩa launch-only và boot

```bash
python scripts/run_qemu.py --profile bios --launch-only
python scripts/run_qemu.py --profile uefi --launch-only
```

TCG, 1 vCPU, 256 MiB, display/network off; BIOS pc/SeaBIOS, UEFI q35/OVMF. Launch-only thêm -S, QMP greeting → capabilities → query-status phải có running=false, rồi quit/reap sạch. PASS chỉ là **emulator launch**, không chứng minh firmware execution hoặc Lamix OS boot. Không tạo bootable image trong M02.

Boot skeleton: khi chưa có image, CLI ghi SKIPPED và exit 77; CTest dùng SKIP_RETURN_CODE 77. Khi có image ở milestone sau, dùng CLI --image cùng --success-marker và --panic-marker hoặc các biến LAMIX_TEST_IMAGE/LAMIX_TEST_SUCCESS_MARKER/LAMIX_TEST_PANIC_MARKER. Image được chỉ định nhưng không tồn tại/không phải regular file hoặc thiếu marker là LAUNCH_ERROR, không SKIP. Không chấp nhận block device. Raw image dùng snapshot; UEFI CODE read-only và VARS sao chép riêng cho từng run. Override --ovmf-code/--ovmf-vars phải cung cấp cùng cặp, không trộn biến thể. --bios chỉ định SeaBIOS khác khi cần.

### Kết quả và cleanup

Marker UTF-8 literal, khớp toàn dòng serial, không regex; chuẩn hóa CRLF và xử lý chunks. Marker cụ thể OS chưa đóng băng. Marker phải khác nhau, không rỗng hoặc chứa newline. Dòng serial quá dài được bỏ qua khi tìm marker, raw log vẫn giữ; đọc tăng dần với bộ đệm giới hạn.

Kết quả JSON phân biệt PASS, SKIPPED, PANIC, TIMEOUT, EXIT, LAUNCH_ERROR, PROTOCOL_ERROR và CLEANUP_ERROR. Exit CLI 0 chỉ khi PASS, 77 khi SKIPPED, 1 khi harness thất bại; lỗi cú pháp argparse có thể exit 2. Success chỉ là ứng viên: panic trong final drain có ưu tiên, exit lỗi không bị che bởi success; exit 0 thiếu marker vẫn FAIL.

Deadline monotonic mặc định launch 10 giây, boot 30 giây; --timeout ghi đè bằng số hữu hạn dương. Cleanup ưu tiên QMP quit, tiếp tục drain stdout để tránh pipe đầy, sau đó terminate group và kill với grace 2 giây khi cần. Luôn reap tiến trình trực tiếp; PASS không cho phép shutdown cưỡng bức hoặc group còn sống. TIMEOUT vẫn là TIMEOUT sau kill, không thành PASS. Exception cũng đi qua cleanup/final log.

Mỗi run dùng thư mục riêng out/m02/run-... (hoặc --output-root), gồm serial.log, stderr.log, qmp.log, result.json và bản VARS nếu dùng UEFI. JSON có backend fixture/qemu để phân biệt tiến trình host giả với emulator thật, profile/mode, command, PID/return code, elapsed, forced cleanup và lý do; không dump environment/secrets. Không xóa logs lỗi hoặc thư mục run khác. Tests fixtures dùng dữ liệu giả/tiến trình host, không phải guest boot; tests launch thật kiểm tra firmware gốc không đổi.

### Harness CI và bằng chứng

[Harness CI](../.github/workflows/harness-ci.yml): push main, PR main, workflow_dispatch, ubuntu-22.04/Python 3.12, 10 phút, contents: read, checkout không lưu credential, actions ghim SHA. Cài QEMU/SeaBIOS/OVMF và lockfile host, không cần KVM/cross-compiler.

Job chạy CTest, xuất fixtures.xml, launch.xml, ctest.xml và summary từ XML thực. Artifact m02-reports giữ JUnit/result.json; khi lỗi thêm m02-failure-logs. Không upload firmware, image hoặc compiler. Phải phân biệt suite thành công với hai boot skeleton bị SKIP; không dùng dòng CTest “100% tests passed” để tuyên bố OS boot.

M02 PASS chỉ khi fixture/cleanup và launch BIOS/UEFI local đạt, boot skeleton SKIP có lý do, regression M01/tài liệu đạt và ba workflows cùng SUCCESS trên SHA nghiệm thu. Run URLs/kết quả thực tế nằm trong handoff; PR/manual trigger không được coi đã test thật nếu chưa chạy.
