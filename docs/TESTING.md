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
Trạng thái milestone: M00=DONE; M01=DONE; M02=DONE; M03=DONE; M04=DONE; M05=DONE; M06=DONE; M07=DONE; M08=DONE; M09=DONE; M10=DONE; M11=DONE; M12=DONE; M13=DONE; M14=DONE; M15=DONE
Trạng thái hiện hành: M16 DONE; M17 DONE; M18 PLANNED; M19 DONE; M20 IN_PROGRESS; M21–M44 PLANNED
```

Giá trị dùng tập trạng thái trong roadmap. Mục milestone không ghi trạng thái riêng mặc định PLANNED; dòng chuẩn phải khớp M00–M15 và giữa ba file. Validator kiểm tra M00–M44 đủ, đúng thứ tự, không trùng; các quy tắc kiến trúc, changelog thật và chất lượng handoff vẫn cần review theo [skill](../.agents/skills/docs-consistency/SKILL.md).

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

## Boot contract M03

Đặc tả tại [BOOT_PROTOCOL](BOOT_PROTOCOL.md) và [IMAGE_LAYOUT](IMAGE_LAYOUT.md). Chạy từ root sau khi activate .venv:

```bash
cmake -S tests/boot -B build/m03 -G Ninja -DCMAKE_BUILD_TYPE=Debug -DPython3_EXECUTABLE="$PWD/.venv/bin/python"
cmake --build build/m03
ctest --test-dir build/m03 --no-tests=error --output-on-failure --output-junit "$PWD/out/m03/ctest-Debug.xml"
cmake -S tests/boot -B build/m03-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DPython3_EXECUTABLE="$PWD/.venv/bin/python"
cmake --build build/m03-release
ctest --test-dir build/m03-release --no-tests=error --output-on-failure --output-junit "$PWD/out/m03/ctest-Release.xml"
cmake -S tests/boot -B build/m03-sanitize -G Ninja -DCMAKE_BUILD_TYPE=Debug -DLAMIX_SANITIZERS=ON -DPython3_EXECUTABLE="$PWD/.venv/bin/python"
cmake --build build/m03-sanitize
ctest --test-dir build/m03-sanitize --no-tests=error --output-on-failure --output-junit "$PWD/out/m03/ctest-sanitize.xml"
python scripts/check_boot_cross.py
build/m03/layout-report > out/m03/layout.json
python -m pytest -q tests -m 'not qemu'
python scripts/smoke.py --reproducible
```

Host CMake dùng compiler Linux và tests C/C++; CTest Python chạy fixtures byte độc lập/negative tests, metadata GPT512/4096 và normalization oracle. Khi pytest chạy trực tiếp, fixture compile shared library vào tmp_path; thiếu compiler là FAIL, không SKIP. CTest có LIB override tới thư viện vừa build. Native runner validate BootInfo hợp lệ, mọi prefix truncation và mọi single-bit mutation của blob, note/UEFI descriptors; assertions vẫn bật trong Release (không dùng assert bị NDEBUG loại bỏ).

ASan/UBSan chạy native suite, không preload sanitizer vào interpreter Python; Python semantic/GPT tests chạy trong Debug/Release. `ASAN_OPTIONS=detect_leaks=0` vì LeakSanitizer không chạy dưới ptrace sandbox và thư viện không heap; vẫn giữ ASan memory checks và `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`. Không coi đây là kiểm chứng leaks ở subsystem OS tương lai.

Cross checker compile static archive bằng x86_64-elf C11/C++17, freestanding/no builtin/stack protector/red-zone/SIMD, không chạy target. `nm -u` phải không có undefined runtime symbols; logs/cross.json tại out/m03. M01 sample và reproducibility inputs không đổi.

[Boot Contract CI](../.github/workflows/boot-contract-ci.yml) chạy host Debug/Release/sanitizer, độc lập cross bootstrap; [Toolchain CI](../.github/workflows/toolchain-ci.yml) thêm cross compile-only và artifact m03-cross-reports. Bốn workflows phải xanh trên đúng SHA nghiệm thu. PR review vẫn bắt buộc trước merge. Artifact m03-reports chỉ XML/JSON/test logs, không upload BootInfo fixture binary, firmware, disk images hoặc compiler.

CMake tạo out/m03; các build directories và reports ignored. Ghi commands/versions/counts và CI URLs tại handoff. Regression harness M02 vẫn gồm hai launch-only paused và hai boot skeleton SKIPPED/77; không có image, firmware execution/CPU state/paging/A-B recovery vẫn NOT RUN. Host validation PASS không phải OS boot PASS.

## BIOS Stage1 M04

Sau activate .venv, từ root:

```bash
cmake -S tests/bios -B build/m04 -G Ninja -DPython3_EXECUTABLE="$PWD/.venv/bin/python"
cmake --build build/m04
ctest --test-dir build/m04 --no-tests=error --output-on-failure --output-junit "$PWD/out/m04/ctest.xml"
```

Build phải qua gate 440 byte trước target probe. CTest stage1-host kiểm size/padding/PMBR, budget assertion thật, reproducibility 2 roots, sparse builder/path/no overwrite và harness mode guards; stage1-qemu chạy 69 cases thật qua default QEMU command builder, không mock BIOS. Thiếu NASM/QEMU/firmware là FAIL, không SKIP. Pytest host trực tiếp assemble vào tmp nếu chưa có override artifact; CTest dùng binary vừa build qua LAMIX_STAGE1_BIN. Marker qemu tránh lặp guest tests trong M01 Debug/Release.

Positive canonical/relocated/exact-window nhận M04:PASS sau probe ABI và 32 KiB patterns/trailer. Negative GPT/header 32 bytes/GUID 16 bytes/bounds/payload/EIO nhận PANIC với reason phù hợp; pytest PASS của expected failure không đổi JSON guest status thành PASS. backend=qemu, mode=bios-stage1, không -S. Snapshot/metadata hashes/firmware hashes và process cleanup đều phải đạt. Các errors D do unsupported EDD/4Kn không có injection riêng, ghi NOT RUN.

Harness Config thêm purpose và test_io_error_sector ở cuối để giữ positional API cũ; CLI chỉ thêm --purpose. Fault chỉ cho Stage1, validate sector trước launch; blkdebug dùng read_aio/errno 5/once off, rerror report. TMPDIR của process con là run directory, xử lý snapshot khi /var/tmp host chỉ đọc; không sửa environment của host.

[Harness CI](../.github/workflows/harness-ci.yml) thêm NASM, build/CTest M04 và m04-reports (XML/JSON/serial/QMP/listing), không bootstrap cross hoặc upload binary/image/firmware. Summary tách M02 paused launch khỏi M04 Stage1, không tuyên bố OS boot. Logs/result.json/case.json và binary.json ở out/m04; baseline/group snapshots local ignored. [BIOS_STAGE1](BIOS_STAGE1.md) có matrix source checks/ABI; run URLs và counts tại handoff.

Regression gồm validator/unittest, pytest -m 'not qemu', M03 CTest/sanitizer/cross, M01 smoke --reproducible và M02 CTest. M04 không gán image OS mặc định: boot-bios/boot-uefi vẫn SKIPPED/77. GDB, actual kernel/UEFI boot và A/B recovery vẫn NOT RUN.

## BIOS Stage2 M05

Đường thật: SeaBIOS → Stage1440 → Stage2 bootstrap/core → ESP FAT32 → ELF64 higher-half probe/LBP. Không phải kernel sản phẩm hoặc UEFI parity. Gate compile/link artifacts riêng không bao giờ dùng làm boot payload.

```bash
. .venv/bin/activate
cmake -S tests/bios_stage2 -B build/m05 -G Ninja -DCMAKE_BUILD_TYPE=Debug -DPython3_EXECUTABLE="$PWD/.venv/bin/python"
cmake --build build/m05 --verbose
ctest --test-dir build/m05 --no-tests=error --output-on-failure --output-junit "$PWD/out/m05/ctest-Debug.xml"
cmake -S tests/bios_stage2 -B build/m05-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DPython3_EXECUTABLE="$PWD/.venv/bin/python"
cmake --build build/m05-release
ctest --test-dir build/m05-release --no-tests=error --output-on-failure --output-junit "$PWD/out/m05/ctest-Release.xml"
cmake -S tests/bios_stage2 -B build/m05-sanitize -G Ninja -DLAMIX_STAGE2_HOST_ONLY=ON -DLAMIX_SANITIZERS=ON -DCMAKE_BUILD_TYPE=Debug -DPython3_EXECUTABLE="$PWD/.venv/bin/python"
ctest --test-dir build/m05-sanitize --no-tests=error --output-on-failure --output-junit "$PWD/out/m05/ctest-sanitize.xml"
```

Host-only CI dùng LAMIX_STAGE2_HOST_ONLY=ON, không compiler cross/NASM/QEMU. CTest native chạy oracle division, parser positive/negative và header mutations; sanitizers chạy native executable, không nạp ASan vào Python ctypes. Host pytest có golden ELF bytes độc lập linker và randomized division oracle. Native sanitizer fixture binaries chỉ tạm, không upload.

Target suite có gate/compiler provenance/size/hash, host tests và guest tests riêng marker qemu. Source/artifact hashes phải khớp build.json PASS; artifact cũ sau build failure không dùng để báo PASS. Config chỉ thêm purpose bios-stage2 và test-only CPU/RAM overrides (64/256/4608 MiB; qemu64 và biến thể no-NX/no-LM); không thay M02/M04 defaults. Fault injection blkdebug được giới hạn BIOS stages; negative JSON giữ PANIC, pytest PASS nghĩa đúng expected reason.

Cases: RAM64/256/>4 GiB, CPU no-NX, core>32 KiB, fragmented directories/kernel, active FAT; header bounds/CRC, GPT backup/array/mirror/profile, FAT loops/free/short/mismatch, ELF/note/features/entry/physical/WX; OOM; EIO thật ở core/FAT/kernel; RO/guard faults vector0e. E820/A20 và malformed BootInfo/map adapters có nhãn test-only, không phải firmware thật tạo lỗi đó.

Mỗi run có result.json/serial/QMP/stderr, case.json expected/actual và protected hashes; positives có layout.json memory/reservations/mappings từ báo cáo serial. BIOS firmware và extents đã ghi của source image phải không đổi, snapshot bật, cleanup không cưỡng bức, process group được reap. Không hash toàn bộ sparse8 GiB hoặc tuyên bố PC thật đã tương thích.

Toolchain CI build/guest Debug và Release dùng cross cache sẵn; Boot Contract CI host/sanitizer; Harness CI giữ M02/M04. Lưu JUnit/logs/maps/listings/JSON, không upload images/firmware hoặc dependencies. Các kết quả thật theo SHA ở [handoff](SESSION_HANDOFF.md).

Regression phù hợp: validator/unittest tài liệu, pytest -m 'not qemu', M03 native/sanitizer/cross và M02/M04 CTest. M01 tiếp tục Toolchain CI; không tự chạy toàn bộ M01 local khi không đổi inputs. OS skeleton mặc định vẫn SKIPPED; M08–M11 DONE. Không gán probe M05–M11 làm image OS sản phẩm mặc định.

## Kernel tối thiểu M08

Build ELF M08 dùng GCC cross, GNU LD/Binutils và NASM hiện có; không bootstrap lại M01. Build report xác minh ELF ET_EXEC x86_64, entry higher-half, load segments `p_paddr=0`, không PT_DYNAMIC/PT_INTERP hoặc undefined symbols, cùng hashes nguồn/artifact:

```bash
python tests/m08/build_kernel.py --build build/m08
python tests/m08/build_kernel.py --build build/m08-ud --fault ud
python tests/m08/build_kernel.py --build build/m08-pf --fault pf
PATH="$PWD/.venv/bin:$PATH" .venv/bin/cmake -S tests/m08 -B build/m08-tests -G Ninja -DPython3_EXECUTABLE="$PWD/.venv/bin/python"
PATH="$PWD/.venv/bin:$PATH" .venv/bin/cmake --build build/m08-tests
PATH="$PWD/.venv/bin:$PATH" .venv/bin/ctest --test-dir build/m08-tests --no-tests=error --output-on-failure --output-junit "$PWD/out/m08/ctest.xml"
```

M08 mở rộng `scripts/m07_image.py` bằng `--kernel-build <dir>` cho create/check; nếu bỏ tùy chọn, image vẫn dùng kernel M05 như trước. CTest host kiểm provenance/layout và cấu trúc descriptor/vector/PIC/PIT; guest tạo image M07 có kernel M08 và chạy cùng image/kernel qua SeaBIOS/OVMF. Profile normal phải vào LBP v1, nhận IRQ0/PIT ba tick và phát `M08:PASS`; `ud`/`pf` phải dừng `M08:PANIC` đúng vector 6/14, PF báo CR2 guard. Expected PANIC không được đổi thành guest PASS.

Khi LBP có ACPI RSDP, kernel ánh xạ vật lý RSDP vào scratch VA đọc-only, cung cấp `lbp_span` cho `lbp_validate()`, rồi gỡ ánh xạ trước khi kiểm tra các mapping cuối. Điều này không đổi LBP wire format. Profile OVMF local hiện bỏ ACPI vì RSDP không căn 16 byte; do đó guest path có ACPI hợp lệ **NOT RUN**. Host structural test kiểm tra đường tạo span và gỡ mapping; regression M03 kiểm tra validator/span.

Harness dùng QEMU snapshot, OVMF CODE read-only, VARS riêng, QMP/serial/deadline/cleanup; test hash protected image extents, firmware sources và build artifacts trước/sau. Reports ở `out/m08/`; không lưu image/firmware trong CI. Host sanitizer được kế thừa từ M03 LBP library CTest; kernel target không chạy dưới host sanitizer. PC thật, phần cứng PIC/PIT và firmware ngoài profile ghi **NOT RUN**.

M05 có thêm adapter inject-segments thiết lập hidden limits64 KiB và fixture large-core/large-core-crc xuyên biên64 KiB; xem [review segment bounds](BIOS_STAGE2.md#review-segment-bounds). Đây là kiểm tra real-mode bootstrap, không giả định BIOS để lại unreal-mode cache.

## UEFI loader M06

Build PE/COFF cần Clang, `lld-link`, NASM; GCC cross vẫn build BIOS/kernel ELF. Script chỉ ghi trong `build/`, không cài dependency:

```bash
python3 tests/uefi/build_uefi.py --build build/m06
cmake -S tests/uefi -B build/m06-tests -G Ninja -DPython3_EXECUTABLE="$PWD/.venv/bin/python"
cmake --build build/m06-tests
ctest --test-dir build/m06-tests --no-tests=error --output-on-failure --output-junit "$PWD/out/m06/ctest.xml"
```

PE inspection kiểm PE32+ x64, EFI application subsystem, entry, relocations và imports rỗng. Guest CTest tạo fixture mới; OVMF CODE read-only, VARS copy theo run, disk snapshot, serial/QMP, deadline, cleanup và hashes bảo vệ source extents/firmware. Guest dương phải thật sự chạy `BOOTX64.EFI`, gọi `ExitBootServices()` và phát `M06:PASS` từ kernel probe; launch-only không được tính. Negative cases gồm file thiếu, ELF/feature negotiation và GPT identity sai; các adapter không mô phỏng lỗi phần cứng. Reports tại `out/m06/`, không upload firmware hoặc disk images. ACPI bị bỏ nếu không hợp LBP v1; OVMF local hiện có RSDP không căn 16. PC thật, 4Kn, PixelBltOnly/firmware khác và lỗi ExitBootServices do firmware ngoài test profile ghi `NOT RUN`.

Toolchain CI chạy PE + guest OVMF trên SHA của thay đổi; M05 BIOS Debug/Release, M03 contract tests/sanitizer và M02/M04 harness được chạy hồi quy riêng. Kiểm tra release vẫn cần CI xanh đúng commit, review và xác nhận hoàn thành; kết quả local không thành CI SUCCESS.


## Image và boot parity M07

Generator/checker cần artifact M05/M06 PASS, `mkfs.fat`, `mmd`, `mcopy`, `mdir`, QEMU, SeaBIOS và OVMF. Chúng không build compiler, không mount image và chỉ tạo image mới dưới `build/`, `out/` hoặc `/tmp`:

```bash
. .venv/bin/activate
python tests/bios_stage2/build_stage2.py --build build/m07-input --profile Debug
python tests/uefi/build_uefi.py --build build/m06
python scripts/m07_image.py create --image out/m07/lamix.img \
  --bios-build build/m07-input --uefi-build build/m06 --seed local-check
python scripts/m07_image.py check --image out/m07/lamix.img \
  --bios-build build/m07-input --uefi-build build/m06
cmake -S tests/m07 -B build/m07-tests -G Ninja \
  -DPython3_EXECUTABLE="$PWD/.venv/bin/python"
ctest --test-dir build/m07-tests --no-tests=error --output-on-failure \
  --output-junit "$PWD/out/m07/ctest.xml"
```

Profile giữ GPT 8 GiB, ESP FAT32 256 MiB; chỉ ESP có payload `BOOTX64.EFI` và `EFI/LAMIX/A/KERNEL.ELF`. GUID ngẫu nhiên nếu bỏ `--seed`, deterministic với seed; B/User Data không được format và loader tiếp tục chọn A tường minh. Checker đối chiếu GPT/CRC/layout, FAT32 paths, Stage1/Stage2, EFI và kernel hashes với build artifacts.

Guest parity chạy **cùng raw image** qua SeaBIOS và OVMF, kiểm marker firmware thật và dòng `M07:LBP` do cùng kernel probe phát sau khi validator LBP v1, CPU/paging/stack/guard/mapping checks đạt. So sánh version, slot A, required features và NX; source boot device và optional feature records có thể khác giữa firmware. Negative guest dùng sparse clone làm hỏng ELF kernel, giữ nguyên FAT boot path, rồi yêu cầu cả BIOS/UEFI từ chối đúng nhóm `ELF`. M05/M06 CTest hồi quy tiếp tục chạy parser/GPT/ELF/feature negative cases đầy đủ. Protected extents gồm PMBR/GPT, BIOS Boot Partition và ESP; firmware hashes cùng cleanup được kiểm tra. Không hash cả disk sparse 8 GiB; không upload image/firmware.

`out/m07/` giữ XML/JSON/serial/QMP/report cục bộ. CI chỉ upload reports và build manifest. Boot PASS ở OVMF/SeaBIOS không chứng minh tương thích PC thật; 4Kn, slot B, filesystem System/User, updates/fallback/modules sản phẩm và Secure Boot tiếp tục ngoài M07.

## Bitmap PMM M09

M09 kernel dùng GCC cross/Binutils và NASM hiện có; không bootstrap M01. Build report ghi compiler/linker, command, source hashes, ELF inspection và SHA-256. Image M07 nhận provenance `m09-kernel` qua `kernel_build`; đường mặc định tiếp tục dùng M05, profile M08 vẫn hợp lệ:

```bash
python tests/m09/build_kernel.py --build build/m09
PATH="$PWD/.venv/bin:$PATH" .venv/bin/cmake -S tests/m09 -B build/m09-tests -G Ninja -DPython3_EXECUTABLE="$PWD/.venv/bin/python"
PATH="$PWD/.venv/bin:$PATH" .venv/bin/cmake --build build/m09-tests
PATH="$PWD/.venv/bin:$PATH" .venv/bin/ctest --test-dir build/m09-tests --no-tests=error --output-on-failure --output-junit "$PWD/out/m09/ctest.xml"
```

Host suite chạy cùng `kernel/pmm.c` với fixture LBP đã validate, kiểm nhiều usable extents và holes, giới hạn 4 GiB, lỗi map/storage, OOM, invalid/double-free và stress. CTest sanitizer chạy native PMM cùng `lib/boot/contract.c` dưới ASan/UBSan.

Guest test tạo image mới với cùng kernel ELF cho SeaBIOS/OVMF; yêu cầu `M08:PASS` và `M09:PASS`, tám frame duy nhất thuộc usable, free count trở lại sau free và free lỗi bị từ chối. QEMU dùng snapshot, OVMF CODE nguồn chỉ đọc, VARS riêng, serial/QMP, deadline/cleanup; hash image extents/firmware/build inputs trước/sau phải bằng nhau. Guest là boot thực, không phải launch-only. Reports tại `out/m09/`, không đưa image hoặc firmware vào artifact.

M09 **DONE**: local CTest 3/3, hai firmware guest PASS và bốn workflow CI SUCCESS trên SHA nghiệm thu; kết quả chi tiết tại [handoff](SESSION_HANDOFF.md). RAM trên 4 GiB, reclaim LOADER/ACPI, PC thật và firmware ngoài QEMU profile **NOT RUN/ngoài phạm vi**. VMM/mapping, zeroing, contiguous allocator, SMP và kernel sản phẩm thuộc phạm vi sau M09.

## VMM kernel address space M10

Build M10 dùng GCC cross/Binutils và NASM hiện có; không bootstrap M01. Build report ghi command, tool versions, source hashes, ELF inspection và SHA-256. Image M07 nhận provenance `m10-kernel`; kernel mặc định M05 và profile M08/M09 vẫn giữ nguyên:

```bash
python tests/m10/build_kernel.py --build build/m10
PATH="$PWD/.venv/bin:$PATH" .venv/bin/cmake -S tests/m10 -B build/m10-tests -G Ninja -DPython3_EXECUTABLE="$PWD/.venv/bin/python"
PATH="$PWD/.venv/bin:$PATH" .venv/bin/cmake --build build/m10-tests
PATH="$PWD/.venv/bin:$PATH" .venv/bin/ctest --test-dir build/m10-tests --no-tests=error --output-on-failure
```

CTest gồm native ASan/UBSan trên implementation VMM thật, host provenance/image checks và 8 guest boots trên cùng kernel profile set: normal, ghi RO, execute NX và chạm guard, từng profile qua SeaBIOS lẫn OVMF. Guest âm yêu cầu `#PF` vector `0e`, CR2/error bits đúng; normal yêu cầu `M08:PASS`, `M09:PASS`, `M10:PASS`. QEMU dùng snapshot image, firmware CODE chỉ đọc, VARS riêng, serial/QMP, timeout và cleanup; reports tại `out/m10/`, không upload image/firmware.

M10 **DONE**: local CTest **3/3 PASS** sau sửa review, guest **8/8 PASS**, image/provenance và sanitizer PASS. Bốn GitHub workflows SUCCESS trên PR head `7e0b2f288e0497deb4a2c5fd4a50c679643643ba`. Tag `milestone/M10` giữ nguyên tại commit đầu `dd6365c6`; review-fix commit nằm trong merge PR #8. PC thật, firmware ngoài QEMU, SMP, RAM trên 4 GiB và process address spaces **NOT RUN/ngoài phạm vi**.

## Kernel heap M11

Kernel M11 nối `kernel/heap.c` với PMM/VMM sau khi root CR3 mới hoạt động. Build dùng GCC/G++ cross, GNU Binutils và NASM hiện có; không bootstrap M01. Build report ghi commands, versions, source/artifact hashes và ELF inspection. M07 image tool nhận provenance `m11-kernel`, mặc định M05 và profiles M08–M10 không đổi:

```bash
python tests/m11/build_kernel.py --build build/m11
PATH="$PWD/.venv/bin:$PATH" .venv/bin/cmake -S tests/m11 -B build/m11-tests -G Ninja -DPython3_EXECUTABLE="$PWD/.venv/bin/python"
PATH="$PWD/.venv/bin:$PATH" .venv/bin/cmake --build build/m11-tests
PATH="$PWD/.venv/bin:$PATH" .venv/bin/ctest --test-dir build/m11-tests --no-tests=error --output-on-failure --output-junit "$PWD/out/m11/ctest.xml"
```

Native ASan/UBSan chạy cùng heap implementation qua simulator map/release callbacks. Host/provenance test kiểm build `m11-kernel`, source hashes, cross G++ consumer và ELF không undefined runtime symbols/static constructors. Guest suite tạo một image/kernel chung rồi boot thật qua SeaBIOS và OVMF; cần marker `M08:PASS`, `M09:PASS`, `M10:PASS`, `M11:PASS`, protected hashes không đổi và cleanup không cưỡng bức. Reports tại `out/m11/`; không upload image hoặc firmware. Host cases bao gồm OOM rollback/exhaustion, overflow/zero-size, alignment, zeroing, reuse/coalescing, invalid pointer và double-free.

M11 **DONE**: CTest **3/3 PASS**, guest BIOS/OVMF **2/2 PASS**; hồi quy M07–M10 PASS. Bốn workflow GitHub Actions SUCCESS trên SHA nghiệm thu `3bf87334999a794bce6dec7e8d691f5f125ff781`; URLs ở [handoff](SESSION_HANDOFF.md). PC thật và firmware ngoài profile **NOT RUN**; global `new/delete`, SMP, IRQ-safe allocation, buddy/slab, user heap và kernel sản phẩm ngoài phạm vi.

## Buddy PMM M12

Buddy được build trong profile riêng `m12-kernel`; bitmap PMM vẫn mặc định ở các profile trước. Host suite biên dịch `kernel/pmm.c` trực tiếp với ASan/UBSan; benchmark native `-O2` so sánh cùng trace xác định với bitmap adapter hỗ trợ contiguous blocks. Workload ghi seed/hash, init latency, metadata, free blocks theo order, largest block và trường hợp order cấp phát thất bại dù free total đủ. Latency và throughput chỉ tính lần allocator thực sự được gọi; no-op bị bỏ qua và có số đếm riêng. Host measurements không phải guest performance.

Lệnh sau khi activate `.venv`:

```bash
python3 tests/m12/build_kernel.py --build build/m12
cmake -S tests/m12 -B build/m12-tests -G Ninja -DPython3_EXECUTABLE="$PWD/.venv/bin/python"
cmake --build build/m12-tests
ctest --test-dir build/m12-tests --no-tests=error --output-on-failure
```

M12 CTest trong Toolchain CI trên acceptance SHA `f3391d66eabc6b6d82a27800db6e229bb74f577b` **6/6 PASS**: Buddy native ASan/UBSan (extents, holes, reservations, OOM, coalescing, invalid/double-free, order18 1 GiB, stress), host provenance, benchmark và guest BIOS/OVMF cùng kernel. Guest BIOS/UEFI phát marker M08–M12, protected hashes giữ nguyên và cleanup không cưỡng bức. CI run [41084497139](https://github.com/duclam2011dz/LamixOS/actions/runs/41084497139); bốn workflow SUCCESS trên SHA này. Benchmark seed `0x12a110c`, trace hash `3ded994161041ff7`, 20,000 trace operations × 5; no-op được bỏ khỏi latency/throughput và báo riêng. Host CI run ghi nhận throughput bitmap cao hơn Buddy; fragmentation fixture bằng nhau. Host-only numbers không kết luận hiệu năng guest; Buddy không được chọn làm mặc định. PC thật/firmware khác **NOT RUN**. M12 DONE.

## ACPI PCI device framework M13

Lệnh từ root workspace sau khi cài đặt môi trường `.venv` sẵn có:

```bash
PATH="$PWD/.venv/bin:$PATH" .venv/bin/cmake -S tests/m13 -B build/m13-tests -G Ninja -DPython3_EXECUTABLE="$PWD/.venv/bin/python"
PATH="$PWD/.venv/bin:$PATH" .venv/bin/cmake --build build/m13-tests
PATH="$PWD/.venv/bin:$PATH" .venv/bin/ctest --test-dir build/m13-tests --no-tests=error --output-on-failure
```

Native ASan/UBSan kiểm RSDP/RSDT/XSDT/MADT/MCFG, checksum/length/entry sai, RSDT fallback, ECAM nonzero start bus, config failure, multifunction enumeration, 64-bit BAR sizing, MMIO range và device/DMA ownership token. Guest suite tạo image/kernel riêng rồi boot thật cùng kernel qua SeaBIOS `pc` và OVMF `q35`; cần PCI enumeration, BAR sizing, uncached MMIO map/query/unmap, M08–M13 serial markers, protected hashes không đổi và cleanup sạch. Không có driver chức năng, DMA buffer hay bus mastering.

Local CTest **3/3 PASS**; hai guest tìm 5 PCI functions, đăng ký 5 devices và map/query/unmap một BAR MMIO. Hồi quy local M07 2/2, M08 2/2, M09 3/3, M10 3/3, M11 3/3, M12 6/6 **PASS**; docs validator và unittest **40/40 PASS**. Bốn GitHub workflows SUCCESS trên acceptance SHA `12d3ef1ac9040f9d523bb05a82066dadcaec6ecf`: [Toolchain](https://github.com/duclam2011dz/LamixOS/actions/runs/41170371900), [Harness](https://github.com/duclam2011dz/LamixOS/actions/runs/41170371899), [Documentation](https://github.com/duclam2011dz/LamixOS/actions/runs/41170371842), [Boot Contract](https://github.com/duclam2011dz/LamixOS/actions/runs/41170371849). M13 **DONE**; PR #11 merged; tag `milestone/M13` trỏ acceptance SHA.

## ACPI, PCI và device framework M13

Profile M13 dùng kernel M09–M12 hiện có và giữ bitmap PMM làm backend đường VMM/heap; Buddy chỉ chạy self-test hồi quy. Kernel lấy RSDP từ LBP v1 nếu loader cung cấp, đọc bảng qua callback VMM uncached, kiểm tra root/table checksum và MADT entries. PCI dùng config mechanism #1 trên segment 0 và ECAM trong MCFG regions; BAR probe tạm dừng I/O, memory decode và bus mastering, lưu/khôi phục BAR/command rồi tạo device registry. MMIO API map uncached qua VMM, không sở hữu frame PMM. DMA registry chỉ lưu owner/generation.

Lệnh kiểm tra local:

```bash
PATH="$PWD/.venv/bin:$PATH" .venv/bin/cmake -S tests/m13 -B build/m13-tests -G Ninja -DPython3_EXECUTABLE="$PWD/.venv/bin/python"
PATH="$PWD/.venv/bin:$PATH" .venv/bin/cmake --build build/m13-tests
PATH="$PWD/.venv/bin:$PATH" .venv/bin/ctest --test-dir build/m13-tests --no-tests=error --output-on-failure
```

Native ASan/UBSan kiểm RSDP/XSDT/MADT/MCFG, checksum/length/entry lỗi, ECAM offset, config failure, multifunction enumeration, 64-bit BAR sizing, resource map bounds, registry duplicate/ownership và generation token. Guest cùng kernel qua SeaBIOS `pc` và OVMF `q35` phải boot thật, enumerate PCI, probe BAR, map/query/unmap MMIO, phát M08–M13 markers, giữ protected hashes và cleanup sạch. Local CTest **3/3 PASS**; hai guest đều tìm 5 PCI functions và map MMIO BAR thành công.

Giới hạn: hai firmware guest không phát ACPI record hợp lệ; serial báo `M13:ACPI:ABSENT`. Guest ACPI traversal **NOT RUN**; parser malformed cases được chứng minh bằng host sanitizer fixtures. Đây không được trình bày là guest ACPI PASS. PC thật và firmware ngoài profile QEMU **NOT RUN**. M13 DONE theo acceptance Roadmap; đổi cách firmware/loader cấp ACPI record cần kế hoạch và duyệt riêng.

## Local APIC/IOAPIC và timer M14

```sh
cmake -S tests/m14 -B build/m14-tests -G Ninja -DPython3_EXECUTABLE="$PWD/.venv/bin/python"
cmake --build build/m14-tests
ctest --test-dir build/m14-tests --no-tests=error --output-on-failure
```

Native ASan/UBSan kiểm tra RSDP revision/checksum/alignment, MADT entry/BSP/IOAPIC/IRQ override và malformed bounds, APIC redirection cùng giới hạn hiệu chuẩn. Guest dùng cùng kernel/image qua SeaBIOS `pc` và OVMF `q35`, mỗi profile ba lần boot. Yêu cầu serial markers M08–M14, ACPI/APIC/IOAPIC/PIT reference/LAPIC timer, PIC mask, đủ timer ticks, không EOI trùng; image/kernel/firmware hashes phải giữ nguyên và cleanup sạch. Build-only không phải guest boot.

Kernel cross-build, BIOS Stage2 build-only (core 172,032/262,144 byte; bootstrap 32 KiB), UEFI build-only, native sanitizer và M14 CTest **3/3 PASS**. Guest SeaBIOS `pc` và OVMF `q35` cùng kernel/image **3/3 PASS mỗi profile**; các marker ACPI/APIC/IOAPIC/PIT reference/LAPIC timer/M14 đều có, protected hashes giữ nguyên, cleanup sạch. Regression M05 **4/4 PASS** (gồm kernel yêu cầu ACPI) và CTest M07 2/2, M08 2/2, M09 3/3, M10 3/3, M11 3/3, M12 6/6, M13 3/3 **PASS**. Docs validator **PASS**, unittest **41/41 PASS**. GitHub CI **PENDING**; M14 chưa DONE. PC thật và firmware ngoài QEMU profile **NOT RUN**.


## Input bàn phím và chuột PS/2 M15

```sh
PATH="$PWD/.venv/bin:$PATH" .venv/bin/cmake -S tests/m15 -B build/m15-tests -G Ninja -DPython3_EXECUTABLE="$PWD/.venv/bin/python"
PATH="$PWD/.venv/bin:$PATH" .venv/bin/cmake --build build/m15-tests
PATH="$PWD/.venv/bin:$PATH" .venv/bin/ctest --test-dir build/m15-tests --no-tests=error --output-on-failure
```

Native ASan/UBSan dùng cùng decoder/queue implementation, kiểm tra make/break, phím mở rộng, Shift/Caps, ASCII, mouse sign/button/overflow, queue wrap/sequence/drop. QMP harness inject input sau `M15:READY`; guest yêu cầu keyboard, mouse motion và button press/release, thứ tự key sequence, không overflow, marker M15, protected hashes và cleanup. Guest nghiệm thu là SeaBIOS `pc` và OVMF `q35`, ba boot mỗi firmware trên cùng kernel/image; host simulation/compile-only không được tính boot.

M15 local: sanitizer/host **2/2 PASS**, guest **3/3 PASS mỗi firmware**, QMP harness **35/35 PASS**, hồi quy M07–M14 PASS và docs validator PASS. Kernel cross-build PASS. Bốn workflow GitHub Actions SUCCESS trên acceptance SHA `831748b5d65f0a88394745b20da82a56f69439c4`: Toolchain [41180666554](https://github.com/duclam2011dz/LamixOS/actions/runs/41180666554), Harness [41180666561](https://github.com/duclam2011dz/LamixOS/actions/runs/41180666561), Documentation [41180666589](https://github.com/duclam2011dz/LamixOS/actions/runs/41180666589), Boot Contract [41180666560](https://github.com/duclam2011dz/LamixOS/actions/runs/41180666560). PC thật và firmware ngoài profile **NOT RUN**. M15 DONE.

## Software framebuffer renderer M16

```sh
PATH="$PWD/.venv/bin:$PATH" .venv/bin/cmake -S tests/m16 -B build/m16-tests -G Ninja -DPython3_EXECUTABLE="$PWD/.venv/bin/python"
PATH="$PWD/.venv/bin:$PATH" .venv/bin/cmake --build build/m16-tests
PATH="$PWD/.venv/bin:$PATH" .venv/bin/ctest --test-dir build/m16-tests --no-tests=error --output-on-failure --output-junit "$PWD/out/m16/ctest.xml"
```

CTest chạy native ASan/UBSan trên cùng `kernel/graphics.c`, host kernel provenance và guest BIOS/UEFI. Guest dùng QMP để hoàn tất input PS/2 M15 trước renderer; cùng kernel/image phải boot qua SeaBIOS `pc` và OVMF `q35`, thấy framebuffer record hợp lệ, render/present và checksum, giữ protected hashes và cleanup sạch. BIOS Stage2 M16 được build bằng `--m16-video`, kiểm tra VBE handoff và hard budgets. Host checksum không thay guest boot; compile/launch-only không tính là guest PASS. Reports ở `out/m16/`, không upload disk image hoặc firmware.

M16 nghiệm thu: CTest **3/3 PASS**, guest SeaBIOS `pc` và OVMF `q35` **1/1 mỗi firmware**; BIOS Stage2 core/BSS **176,128/262,144 byte**, bootstrap **32 KiB**. Regression M07 + Stage2 host + M15 host **54 PASS, 57 deselected**; docs validator **PASS**, unittest tài liệu **41/41 PASS**. PR #14 merge commit `e37b0b6c3f5423ae518c32e2fc325a2dd5a4a19b`, acceptance SHA `09c4248dece6273295c28639090883d00a3af9c0`; bốn workflow SUCCESS: Toolchain [41197034264](https://github.com/duclam2011dz/LamixOS/actions/runs/41197034264), Harness [41197034242](https://github.com/duclam2011dz/LamixOS/actions/runs/41197034242), Documentation [41197034318](https://github.com/duclam2011dz/LamixOS/actions/runs/41197034318), Boot Contract [41197034328](https://github.com/duclam2011dz/LamixOS/actions/runs/41197034328). Tag `milestone/M16` object `a66b7f9a0fbf1d52cb3df1be2b9d09e0c0862a63`. PC thật/firmware ngoài profile QEMU **NOT RUN**. M16 DONE.

## VirtIO-GPU 2D M17

```sh
PATH="$PWD/.venv/bin:$PATH" .venv/bin/cmake -S tests/m17 -B build/m17-tests -G Ninja -DPython3_EXECUTABLE="$PWD/.venv/bin/python"
PATH="$PWD/.venv/bin:$PATH" .venv/bin/cmake --build build/m17-tests
PATH="$PWD/.venv/bin:$PATH" .venv/bin/ctest --test-dir build/m17-tests --no-tests=error --output-on-failure --output-junit "$PWD/out/m17/ctest.xml"
```

CTest gồm `m17-native-sanitize`, host provenance và guest. Host dùng cùng `kernel/virtio_gpu.c` với ASan/UBSan; guest BIOS/UEFI boot image M17 không cần QMP input, chụp QMP screenshot sau `M17:READY`, kiểm tra protected hashes/cleanup và chạy cả profile không gắn VirtIO-GPU để chứng minh fallback. M15 synthetic keyboard/mouse test chạy riêng trong M15 guest suite. Ảnh PPM gốc và PNG review được giữ trong `out/m17/runs/`; kết quả JSON ở `out/m17/results/`. CI chỉ upload reports, serial/QMP logs và screenshots; không upload image hoặc firmware.

CTest local **3/3 PASS**: native sanitizer, host provenance và guest. Guest pytest **4/4 PASS** (GPU BIOS/UEFI và fallback BIOS/UEFI). Native sanitizer bao gồm queue errors, ring index wrap, DMA reset timeout và response đến muộn trong persistent storage. Ảnh BIOS và UEFI đã được mở kiểm tra; cả hai thể hiện frame M17 với nền tối, panel xanh, chữ trắng và đường màu cam. Hồi quy M15 host/sanitizer 2/2, M16 host/sanitizer 2/2, M07 host 7/7 (một QEMU case deselected) và QMP fixtures 1/1 PASS. M07 CTest wrapper trỏ build provenance cũ nên lần chạy đó FAIL; chạy lại suite trực tiếp với Stage2/UEFI build mới đạt. Docs validator **PASS**, unittest **41/41 PASS**. PR #15 merge commit `8afa222fc493c2adbac71bb415adacc03b4510f7`; acceptance SHA `292eb59f3c368c2933a7dbb0d135b89f4fb74415`, tag `milestone/M17`. Bốn workflow trên SHA này SUCCESS: Toolchain [41200982371](https://github.com/duclam2011dz/LamixOS/actions/runs/41200982371), Harness [41200982402](https://github.com/duclam2011dz/LamixOS/actions/runs/41200982402), Documentation [41200982454](https://github.com/duclam2011dz/LamixOS/actions/runs/41200982454), Boot Contract [41200982320](https://github.com/duclam2011dz/LamixOS/actions/runs/41200982320). M17 DONE; PC thật/firmware ngoài profile **NOT RUN**.

### Sửa đường boot trực tiếp M17

- Người dùng báo màn hình đen. Serial dừng ở `M15:READY`, không có `M17:*`: profile M17 gọi M15 self-test trước renderer; test chờ QMP keyboard/mouse events mà lệnh chạy trực tiếp không gửi. M17 guest CI trước đó tự gửi các event này nên không phát hiện lỗi đường chạy trực tiếp.
- Trong profile M17, PS/2 vẫn khởi tạo nhưng self-test tương tác M15 được bỏ qua (`M15:SELFTEST:SKIPPED`); luồng M15 của profile M15 giữ nguyên. M17 guest test không còn gửi input QMP và kiểm tra renderer chạy không cần scripted input.
- M17 CTest sau sửa **3/3 PASS**; guest BIOS/UEFI GPU và fallback **4/4 PASS**. M15 guest regression **2/2 PASS** với build hiện hành. Người dùng xác nhận trực tiếp màn hình đã hiển thị bình thường khi boot cả BIOS và UEFI. Tag `milestone/M17` không bị di chuyển; lần sửa này được ghi nhận như follow-up sau acceptance.
- Thêm `scripts/m17_qemu.py` để build kernel/loaders/image và mở QEMU; image đặt tên theo hash kernel, Stage2 và EFI. Script build-only **PASS**; image do script tạo boot BIOS và UEFI không có QMP input, cả hai phát `M17:PASS` và QMP screenshot xác nhận frame. CTest host có regression cho cấu hình lệnh script. Kernel SHA-256 `d9863e4027da20e1787ab99baf1e4e566ca96750b08a496442ce53d85b00bcf5`; image debug `out/m17/lamix-m17-d9863e4027da-a3ffdf82-c504fc33.img`.

Chạy script từ root workspace (mặc định build image rồi chạy BIOS, sau khi đóng cửa sổ sẽ chạy UEFI):

```sh
./scripts/m17_qemu.py
```

Chọn firmware riêng hoặc chỉ build image:

```sh
./scripts/m17_qemu.py --firmware bios
./scripts/m17_qemu.py --firmware uefi
./scripts/m17_qemu.py --build-only
```

Script yêu cầu toolchain và QEMU/SeaBIOS/OVMF theo [TOOLCHAIN](TOOLCHAIN.md); không tự cài dependencies. Có thể đổi đường dẫn firmware bằng `--seabios`, `--ovmf-code`, `--ovmf-vars` hoặc biến môi trường `LAMIX_SEABIOS`, `LAMIX_OVMF_CODE`, `LAMIX_OVMF_VARS`. Serial hiện trực tiếp trong terminal, VARS riêng và dữ liệu từng lần chạy nằm `out/m17/runs/manual-*`; image dùng snapshot và không bị ghi đè khi build lại.

VirtIO-GPU là VGA adapter duy nhất (`-vga none` trước `virtio-vga`). Giữ terminal mở và đợi serial in `M17:PASS`; OVMF có thể còn hiện thông báo BDS trong lúc handoff. PC thật/firmware ngoài profile QEMU **NOT RUN**.

## Scheduler BSP M19

Script chạy thử tổng quát: `./scripts/lamix_qemu.py` mặc định build profile mới nhất (hiện là M19), tạo image BIOS/UEFI trong `out/m19/`, rồi mở cả hai firmware lần lượt. Dùng `--firmware bios` hoặc `--firmware uefi` để chạy riêng; `--build-only` chỉ build. `--profile m17` chọn profile cũ. Cập nhật `LATEST_PROFILE` khi milestone mới trở thành profile chạy thử mới nhất. Script dùng toolchain/QEMU/SeaBIOS/OVMF đã cài, không cài dependencies. Script M17 cũ vẫn có thể dùng để tái hiện riêng M17.

```sh
./scripts/lamix_qemu.py
./scripts/lamix_qemu.py --firmware bios
./scripts/lamix_qemu.py --firmware uefi
./scripts/lamix_qemu.py --build-only
```

```sh
PATH="$PWD/.venv/bin:$PATH" .venv/bin/cmake -S tests/m19 -B build/m19-tests -G Ninja -DPython3_EXECUTABLE="$PWD/.venv/bin/python"
PATH="$PWD/.venv/bin:$PATH" .venv/bin/cmake --build build/m19-tests
PATH="$PWD/.venv/bin:$PATH" .venv/bin/ctest --test-dir build/m19-tests --no-tests=error --output-on-failure --output-junit "$PWD/out/m19/ctest.xml"
```

`m19-native-sanitize` chạy cùng `kernel/scheduler.c` với ASan/UBSan. Host pytest kiểm build provenance; guest pytest tạo image M19 dựa trên M17 và boot cùng kernel qua SeaBIOS `pc` cùng OVMF `q35`, ba lần mỗi firmware. Guest gắn `virtio-vga`, yêu cầu preemption thật từ hai CPU-bound worker, priority aging đến mức chạy, mutex priority inheritance/direct handoff và wakeup. Serial evidence và JSON ở `out/m19/`; image, firmware và logs không đưa vào Git. Profile chỉ định nghĩa `LAMIX_M19_SCHEDULER`, không build M18.

Trạng thái nghiệm thu M19: native sanitizer+host CTest local **2/2 PASS**, guest BIOS/UEFI **3/3 mỗi firmware PASS**, `m19-kernel` cross-build **PASS**; M19 CTest trong Toolchain CI **3/3 PASS**. Guest phát marker M19, aging đo 300 tick, mutex inheritance/handoff đạt và protected inputs/cleanup sạch. Bốn workflow CI SUCCESS trên acceptance SHA `9a0861ec4b152e189144251ece884bc6ffd466d5`; Codex review clean. PC thật và firmware khác profile QEMU **NOT RUN**. M19 DONE; M18 vẫn PLANNED.

## Processes, ring 3 và cô lập address space M20

Script QEMU tổng quát `./scripts/lamix_qemu.py` hiện mặc định M20, dựng image raw GPT dùng chung BIOS/UEFI trong `out/m20/` rồi mở firmware đã chọn. `--profile m19` và `--profile m17` giữ khả năng chạy lại profile cũ; `--build-only` chỉ dựng artifact.

```sh
./scripts/lamix_qemu.py
./scripts/lamix_qemu.py --firmware bios
./scripts/lamix_qemu.py --firmware uefi
./scripts/lamix_qemu.py --build-only
./scripts/lamix_qemu.py --profile m19 --firmware both
```

CTest M20 gồm `m20-native-sanitize` (implementation thật `process.c`, `vmm.c`, `scheduler.c` với ASan/UBSan), host provenance và guest BIOS/UEFI ba lần mỗi firmware. CI config/build/test:

```sh
PATH="$PWD/.venv/bin:$PATH" .venv/bin/cmake -S tests/m20 -B build/m20-tests -G Ninja -DPython3_EXECUTABLE="$PWD/.venv/bin/python"
PATH="$PWD/.venv/bin:$PATH" .venv/bin/cmake --build build/m20-tests
PATH="$PWD/.venv/bin:$PATH" .venv/bin/ctest --test-dir build/m20-tests --no-tests=error --output-on-failure --output-junit "$PWD/out/m20/ctest.xml"
```

Guest dùng cùng kernel/image trên SeaBIOS `pc` và OVMF `q35`; kiểm tra hai process map cùng user VA tới backing khác nhau, vùng chỉ process B map gây user page fault ở A, B tiếp tục chạy, kernel test manager kill B, wait terminal state và thu hồi resources. QEMU marker và JSON/logs ở `out/m20/`; harness so protected image/firmware/kernel fingerprints và cleanup. Không coi compile-only hoặc host test là ring3 guest proof.

Kết quả hiện tại: `m20-kernel` cross-build và `scripts/lamix_qemu.py --profile m20 --build-only` **PASS**; native ASan/UBSan và host pytest **PASS**; guest BIOS/UEFI **3/3 mỗi firmware PASS**. M19 hồi quy pytest **7/7 PASS**, M19 native sanitizer **PASS**, M17 guest regression **4/4 PASS**. CMake/CTest wrapper local **NOT RUN** vì `cmake` không có trên PATH; docs validator **PASS**, unittest tài liệu **41/41 PASS**. GitHub CI trên acceptance SHA **NOT RUN**. M20 **IN_PROGRESS**, chưa có acceptance SHA/PR/tag. PC thật và firmware ngoài profile QEMU **NOT RUN**.
