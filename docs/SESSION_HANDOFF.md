# Trạng thái M03 — 2026-10-26

Trạng thái milestone: M00=DONE; M01=DONE; M02=DONE; M03=VERIFYING

Kế hoạch M03 đã được người dùng duyệt và triển khai. Contract/thư viện/host tests đã có local PASS; CI của thay đổi M03 NOT RUN trước commit/push, chưa xác nhận hoàn thành. Không tạo tag M03, Release hoặc bắt đầu M04. Bootloader/kernel/bootable image chưa triển khai.

## Git và phạm vi

Repository public/main [duclam2011dz/LamixOS](https://github.com/duclam2011dz/LamixOS), origin HTTPS, GPL-3.0-only. Nhánh triển khai `feat/m03-boot-contract`, base `211ff15c018a159b6df3b8b9cb0e8ccc3f5ba103`; lấy HEAD hiện tại bằng `git rev-parse HEAD`, không tạo commit lặp để ghi SHA của chính handoff. Remote main đã kiểm tra vẫn ở base trước push M03.

Annotated tags local/remote đã xác minh trong khảo sát: M00→aa6aeb9ff8ea4620227020ea43e03f430328d6b6, M01→99f7591046c843d77362eed5f16cf6a34a085bbb, M02→3a4f6206f04287d07b8bd9f29d5cc941af2f50dc. Không chuyển tag M02 sang commit tài liệu base.

M03 thêm headers/assertions C11/C++17, parser/note/record iteration và E820/UEFI normalization C freestanding; GPT host metadata model 512/4096, Debug/Release/native sanitizer/cross checks; Boot Contract CI và Toolchain CI cross step. Đặc tả [BOOT_PROTOCOL](BOOT_PROTOCOL.md), [LBP_LAYOUT](LBP_LAYOUT.md), [IMAGE_LAYOUT](IMAGE_LAYOUT.md), ADR-002. Ba prompt gốc giữ nguyên byte với tag M00; M01 sample/build inputs không đổi. Không disk I/O, firmware calls, boot code hoặc image generator.

## Bằng chứng local

Môi trường WSL2 x86_64; host GCC/G++ 11.4, cross 12.2/Binutils 2.39, Python 3.10.12/pytest 8.3.5, CMake 3.22.6/Ninja 1.11. Activate .venv trước lệnh; đầy đủ tại [TESTING](TESTING.md#boot-contract-m03).

| Kiểm tra chạy thật | Kết quả |
|---|---|
| M03 CTest Debug/Release | PASS, mỗi profile 4 cases, gồm 107 Python host tests và assertions C/C++/native mutations |
| Native ASan/UBSan | PASS, 3 cases; mọi prefix truncation/single-bit mutations BootInfo, note/UEFI mutations và 256 normalization inputs |
| Cross compile-only | PASS C11/C++17, không undefined runtime symbols; compiler/flags provenance kiểm tra từ compile_commands |
| `pytest -q tests -m 'not qemu'` | PASS 184 tests, 2 qemu cases deselected |
| Validator/unittest tài liệu | PASS, 27 tests; trạng thái M03 đồng bộ |
| `scripts/smoke.py --reproducible` | PASS Debug/Release/ELF/provenance và SHA-256 hai source roots theo từng profile |
| CTest harness M02 | PASS 33 fixtures và 2 QEMU launch-only BIOS/UEFI; không cleanup cưỡng bức |
| OS boot BIOS/UEFI | SKIPPED/77 hai skeleton vì chưa có image |
| CPU state, firmware execution, actual paging, A/B recovery | NOT RUN; thuộc các milestone sau |

Logs/JUnit/layout.json/cross.json/cross.log và regression.xml ở out/m03; M01 ở out/m01, M02 ở out/m02, đều ignored. Các command logs chưa gắn SHA trước commit; CI sẽ gắn bằng head_sha. Native sanitizer không chạy Python semantic/GPT tests; chúng chạy ở host Debug/Release. LSan lúc đầu FAIL vì không hoạt động dưới ptrace sandbox; suite đặt detect_leaks=0 vì thư viện không heap, vẫn bật ASan và UBSan halt_on_error. Không coi đó là kiểm chứng leak OS.

## CI và giới hạn

Bàn giao M02 đã kiểm tra qua API: ba workflows SUCCESS trên SHA nghiệm thu 3a4f620 và base 211ff15; URLs ở lịch sử M02/Actions. M03 cần bốn workflows: Documentation, Toolchain (gồm cross), Harness và Boot Contract; trạng thái hiện NOT RUN cho thay đổi chưa push, không suy ra từ local PASS. PR cần review và CI xanh trước merge.

Một số out/m02 logs cũ thiếu backend và có fixture boot/PASS; command gọi fixture_process.py, không phải OS boot. Chỉ dùng JUnit và result.json có backend rõ để báo launch thật. Firmware/image không được upload. Không có KVM; TCG launch-only không chứng minh firmware đã thực thi.

## Next exact action

Review toàn diff/staged và file mới, validator/unittest trước commit; push nhánh M03, mở PR về main, kiểm tra bốn workflows trên đúng SHA. Sau khi CI thành công, cập nhật bằng chứng handoff/PLANS/roadmap và chờ review cùng xác nhận hoàn thành riêng. Không tự merge, tag hoặc sang M04. Nếu cần đổi contract/mapping/partition policy ngoài kế hoạch, dừng phần phụ thuộc để xin duyệt bổ sung.
