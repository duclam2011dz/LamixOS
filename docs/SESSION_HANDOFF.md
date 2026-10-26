# Trạng thái M02 — 2026-10-26

Trạng thái milestone: M00=DONE; M01=DONE; M02=DONE

M02 đã được người dùng phê duyệt kết thúc PASS/DONE. Annotated tag milestone/M02 đã tạo/push và xác minh local/remote. Vẫn ở phiên hiện tại; không chuyển phiên hoặc bắt đầu M03. Bootable image/bootloader/kernel/LBP chưa triển khai.

## Repository và ranh giới

Repository [duclam2011dz/LamixOS](https://github.com/duclam2011dz/LamixOS), Public/main, GPL-3.0-only; origin `https://github.com/duclam2011dz/LamixOS.git`. Tag M00 giữ aa6aeb9ff8ea4620227020ea43e03f430328d6b6; tag M01 giữ 99f7591046c843d77362eed5f16cf6a34a085bbb.

Commit triển khai M02: 68de1934de265fd93ec59613de8e632aa5c31872. Commit sửa CI setup: 42116ea2ba92800384b76e3130ff7f6c5b91b01f. Commit nghiệm thu M02: `3a4f6206f04287d07b8bd9f29d5cc941af2f50dc`. Annotated tag [milestone/M02](https://github.com/duclam2011dz/LamixOS/tree/milestone/M02) giữ nguyên trên SHA này; tag object local/remote khớp nhau và peeled SHA đã được xác minh.

Working tree sạch, main local/remote cùng SHA nghiệm thu trước tạo tag. Bản bàn giao này nằm trong commit tài liệu đóng thủ tục tiếp sau; lấy SHA bằng `git rev-parse HEAD`. Không chuyển tag sang commit tài liệu. Sau push phải xác minh ba workflows SUCCESS trên đúng HEAD, main local/remote đồng bộ và working tree sạch trước báo cáo cuối. Không tạo vòng lặp commit chỉ để tự ghi SHA/run của chính nó.

## Đã hoàn thành

- Thư viện qemu_harness và CLI run_qemu: QMP/serial, monotonic deadline, process-group cleanup, drain, JSON/logs riêng mỗi run. Backend fixture/qemu được gắn nhãn rõ.
- Fixture host không phải guest OS; QEMU thật dùng TCG/1 CPU/256 MiB, pc/SeaBIOS và q35/OVMF, -S và QMP xác nhận running=false rồi quit/reap sạch. Firmware originals không đổi; VARS là copy riêng.
- CTest host-only, marker qemu tránh lặp launch thật trong từng M01 profile, Harness CI riêng không bootstrap compiler. Mở rộng validator/dòng trạng thái M00/M01/M02, giữ nguyên mục tiêu/thứ tự roadmap và ba prompt gốc.

## Bằng chứng nghiệm thu

| Kiểm tra thật | Kết quả |
|---|---|
| Pytest fixture và launch | PASS: 33 fixture tests, 2 QEMU thật paused BIOS/UEFI; gồm split/CRLF, panic ưu tiên, exit/timeout, flood lúc chạy/quit, BrokenPipe, race natural-exit, child/exception cleanup và parallel VARS |
| CTest host-only, JUnit | harness-fixtures/emulator-launch PASS; boot-bios/boot-uefi SKIPPED (77), do chưa có image |
| Artifact GitHub m02-reports | Đã kiểm tra fixtures.xml/launch.xml/ctest.xml và result.json: counts đúng, backend rõ, không có bản ghi qemu boot PASS |
| Regression M01 | PASS Debug/Release, CTest, ELF/link provenance và SHA-256 hai source roots |
| Validator và unittest tài liệu | PASS; 24 tests, trạng thái ba milestone đồng bộ; ba prompt byte-identical với tag M00 |
| [Harness CI 40314941478](https://github.com/duclam2011dz/LamixOS/actions/runs/40314941478) | SUCCESS trên SHA nghiệm thu 3a4f620; artifact đã được kiểm chứng |
| [Documentation CI 40314941426](https://github.com/duclam2011dz/LamixOS/actions/runs/40314941426) | SUCCESS trên cùng SHA nghiệm thu |
| [Toolchain CI 40314941461](https://github.com/duclam2011dz/LamixOS/actions/runs/40314941461) | SUCCESS trên cùng SHA nghiệm thu |
| OS boot BIOS/UEFI | SKIPPED, chưa có image; không tạo boot code/image để vượt gate M02 |

Logs/JSON/JUnit local: out/m02, ignored. Harness CI upload m02-reports (JUnit/result.json), logs chỉ khi job lỗi; không upload firmware/image/compiler. CI của commit tài liệu cuối sẽ được báo sau khi xác minh đúng SHA; tra [Harness](https://github.com/duclam2011dz/LamixOS/actions/workflows/harness-ci.yml), [Toolchain](https://github.com/duclam2011dz/LamixOS/actions/workflows/toolchain-ci.yml) và [Documentation](https://github.com/duclam2011dz/LamixOS/actions/workflows/docs-ci.yml).

## Giới hạn và sự cố đã xử lý

Toolchain CI 40312222548 attempt 1 bị hủy sau hơn 10 phút tải package rất chậm/retry từ azure.archive.ubuntu.com; logs xác nhận chưa tới build. Cancel thường hoàn tất trước yêu cầu force-cancel, nên API force-cancel trả 409 (run đã completed/cancelled). Không sửa Git history/tag. Đã đổi URL Azure sang HTTPS Ubuntu archive trong runner, giữ chữ ký apt/package list và thêm retry/timeout; run tiếp theo SUCCESS. Harness CI dùng cùng lựa chọn mirror, setup timeout 5 phút; Toolchain setup 10 phút. Host local không bị đổi apt sources.

Không có KVM, TCG đã kiểm chứng. Launch-only PASS không chứng minh firmware execution hoặc OS boot. Marker OS chưa đóng băng; image raw/regular file bắt buộc snapshot khi dùng về sau. Override OVMF cần cả CODE/VARS cùng biến thể. PR/manual trigger và GDB guest debug chưa chạy thật. Actions ghim Node 20 được runner ép sang Node 24 có cảnh báo deprecation; chưa coi là lỗi kiểm chứng.

## Bước tiếp theo

Thủ tục tag M02 đã hoàn tất sau phê duyệt của người dùng. Lượt đóng thủ tục chỉ cập nhật README, ROADMAP, PLANS, CHANGELOG và handoff; không sửa code/workflows. Validator và 24 unittest tài liệu phải PASS trước commit; xác minh CI của commit tài liệu theo đúng HEAD trước báo cáo cuối.

Chờ chỉ dẫn tiếp theo của người dùng, vẫn ở phiên hiện tại. Không chuyển phiên, khảo sát/lập kế hoạch hoặc triển khai M03 trong lượt này; M03 giữ PLANNED. Khi được yêu cầu milestone mới, phải dùng Plan Mode thật, đọc AGENTS/handoff/PLANS/đúng mục roadmap và chờ duyệt kế hoạch riêng.
