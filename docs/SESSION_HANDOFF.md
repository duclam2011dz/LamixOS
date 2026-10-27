# Bàn giao M03 — 2026-10-27

Trạng thái milestone: M00=DONE; M01=DONE; M02=DONE; M03=DONE

Người dùng đã phê duyệt kết thúc M03, tạo/push tag và merge ngày 2026-10-27. M03 DONE. Lượt đóng chỉ cập nhật tài liệu; không bắt đầu M04. Bootloader/kernel/bootable image chưa triển khai.

## Repository, merge và tags

Repository public/main [duclam2011dz/LamixOS](https://github.com/duclam2011dz/LamixOS), origin HTTPS, GPL-3.0-only. [PR #1](https://github.com/duclam2011dz/LamixOS/pull/1) đã merge bằng commit `347cc346edc8739feb15fbb38c3b90994d9941a6`, giữ nguyên lịch sử. Phê duyệt được đưa trong chat; không tạo GitHub review thay người dùng.

Annotated [milestone/M03](https://github.com/duclam2011dz/LamixOS/tree/milestone/M03) đã tạo/push và xác minh local/remote: tag object `138293cedea692c4a86c9e493f982182cc81b2c9`, peeled commit nghiệm thu `ab7b1e4bdab199f1368d207a2deff79806bc1433`. Tag giữ SHA này, không chuyển sang merge commit hoặc commit tài liệu đóng thủ tục. M00→aa6aeb9ff8ea4620227020ea43e03f430328d6b6, M01→99f7591046c843d77362eed5f16cf6a34a085bbb, M02→3a4f6206f04287d07b8bd9f29d5cc941af2f50dc giữ nguyên.

Main local/remote đã đồng bộ ở merge commit trước lượt tài liệu cuối, working tree sạch. Bản handoff nằm trong commit tài liệu tiếp sau; lấy SHA thực tế bằng `git rev-parse HEAD`, không tạo vòng lặp commit để ghi SHA/run của chính nó. Sau push phải kiểm tra main local/remote, working tree và bốn workflows trên đúng HEAD trước báo cáo cuối.

## Kết quả M03

Headers C11/C++17 assertions từng field, LBP v1 records/kernel note/entry/ownership; thư viện freestanding parsing/validation/E820/UEFI normalization dùng caller-owned buffers. GPT host metadata model 512/4096, profile 8 GiB; không disk I/O/firmware calls/image generator. Đặc tả tại [BOOT_PROTOCOL](BOOT_PROTOCOL.md), [LBP_LAYOUT](LBP_LAYOUT.md), [IMAGE_LAYOUT](IMAGE_LAYOUT.md), ADR-002; lệnh tại [TESTING](TESTING.md#boot-contract-m03).

| Kiểm tra thực sự đã chạy trong M03 | Kết quả |
|---|---|
| Host Debug/Release | PASS, mỗi profile 108 Python tests; CTest 4 cases gồm assertions C/C++ và native mutations |
| Native ASan/UBSan | PASS, 3 cases; prefix truncation/single-bit BootInfo mutations, note/UEFI mutations và 256 normalization inputs |
| Cross C11/C++17 compile-only | PASS, đúng x86_64-elf/flags theo compile_commands; không undefined runtime symbols |
| Host regression | PASS 185 tests, 2 qemu cases deselected |
| Validator/unittest tài liệu | PASS 27 tests; chạy lại trong lượt đóng thủ tục trước commit |
| M01 | PASS Debug/Release/ELF/provenance và SHA-256 giữa hai source roots |
| M02 | PASS 33 fixtures và 2 QEMU launch-only paused BIOS/UEFI; cleanup sạch |
| OS boot BIOS/UEFI | SKIPPED/77 hai skeleton vì chưa có image |
| CPU state, firmware execution, actual paging, A/B recovery | NOT RUN, thuộc milestones sau |

Môi trường kiểm chứng: WSL2 x86_64, host GCC/G++ 11.4, cross 12.2/Binutils 2.39, Python 3.10.12/pytest 8.3.5, CMake 3.22.6/Ninja 1.11. Logs/JUnit/layout/cross/regression reports local tại out/m03, M01 tại out/m01, M02 tại out/m02, đều ignored. Lượt đóng không chạy lại build/bootstrap/QEMU vì chỉ sửa tài liệu.

## CI và bằng chứng

Bốn workflows đã xác minh SUCCESS trên SHA nghiệm thu `ab7b1e4bdab199f1368d207a2deff79806bc1433`:

- [Documentation CI](https://github.com/duclam2011dz/LamixOS/actions/runs/40352877005).
- [Toolchain CI](https://github.com/duclam2011dz/LamixOS/actions/runs/40352876972).
- [Harness CI](https://github.com/duclam2011dz/LamixOS/actions/runs/40352877288).
- [Boot Contract CI](https://github.com/duclam2011dz/LamixOS/actions/runs/40352876891).

Artifact m03-reports của commit code 66f5b1e (id 12663991967) đã được đối chiếu: mỗi profile 108 tests, không failures/errors/skips, CTest 4/4 và native sanitizer 3/3, layout.json khớp headers; chỉ XML/JSON/logs, không firmware/image/binary. CI commit tài liệu đóng thủ tục được kiểm tra theo HEAD sau push và báo cuối lượt; không suy ra từ CI SHA trước.

## Giới hạn và bước tiếp theo

LSan lúc đầu FAIL do ptrace sandbox; suite dùng detect_leaks=0 vì thư viện không heap, vẫn bật ASan/UBSan. Python semantic/GPT tests chạy Debug/Release, không chạy trong native sanitizer suite. Không có KVM; TCG launch-only không chứng minh firmware execution hoặc OS boot. Một số logs M02 cũ thiếu backend và gọi fixture_process.py, không được coi là guest boot PASS. Ba prompt gốc giữ byte-identical với tag M00; tags M00–M02 không đổi.

Sau khi kiểm tra CI HEAD tài liệu cuối, chờ chỉ dẫn mới của người dùng. **Không khảo sát, lập kế hoạch hoặc triển khai M04 trong lượt đóng này.** Khi người dùng yêu cầu milestone mới, cần Plan Mode thật và kế hoạch riêng được duyệt. Không tự tạo version tag/Release hoặc thay tag M03.
