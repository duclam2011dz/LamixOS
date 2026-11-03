# Bàn giao hiện hành — M27 IN_PROGRESS — 2026-11-03

Trạng thái milestone: M00=DONE; M01=DONE; M02=DONE; M03=DONE; M04=DONE; M05=DONE; M06=DONE; M07=DONE; M08=DONE; M09=DONE; M10=DONE; M11=DONE; M12=DONE; M13=DONE; M14=DONE; M15=DONE
Trạng thái hiện hành: M16 DONE; M17 DONE; M18 PLANNED; M19 DONE; M20 DONE; M21 DONE; M22 DONE; M23 DONE; M24 DONE; M25 DONE; M26 DONE; M27 IN_PROGRESS; M28–M44 PLANNED

## M27 — trạng thái triển khai

- Profile `m27-kernel` dùng ABI minor 1.2 gate; `LATEST_PROFILE=m26`. Init static manifest có service shell, dependency/restart policy và rights subset. `lamix_process_spawn_with_rights()` chỉ nhận rights thuộc capability caller; spawn thường cấp 0 quyền. Init launch/reap/restart shell, service control start/stop/restart. Shell có builtins file/log/service/reboot và external ELF launch. Console kết hợp PS/2, UART và framebuffer M16/M17.
- Reboot syscall flush VFS rồi reset qua `0xCF9`. Test disk là regular file riêng dưới `out/m27/runs/`, GPT User Data FAT32; guest kiểm persistence BIOS/UEFI, image/firmware hashes và GPT/ESP/guard ranges. Guest checks xác nhận file-right denial, rights-escalation denial, service lifecycle, serial/PS2 và framebuffer PPM.
- Cross-build **PASS**; CTest **4/4 PASS**, guest pytest **12/12 PASS** (shell/service BIOS/UEFI 6 lượt; reboot BIOS/UEFI 6 lượt). QEMU harness host **36/36 PASS**. Regressions CTest M17 **3/3**, M19 **3/3**, M20 **3/3**, M21 **3/3**, M22 **4/4**, M23 **3/3**, M24 **4/4**, M25 **4/4**, M26 **4/4 PASS**. Docs validator **PASS**, unittest tài liệu **41/41 PASS**. Artifact: kernel SHA-256 `d40434b5c54984c6a6195fcbd9444f969ca584d714e0c768e29a418acbbf3b11`, initramfs SHA-256 `ba8925f5e478769a2994b01ef7c2d4ca84a5ea454746ee9d25b5eab18ee1079a`; reports `build/m27-guest/kernel/build.json`, `out/m27/results/`, `out/m27/guest.xml`.
- CI trên SHA nghiệm thu, review/merge chưa chạy. M27 vẫn `IN_PROGRESS`; chưa tag hoặc chuyển `LATEST_PROFILE`, giữ M26. M18 vẫn PLANNED; M28 không thuộc lượt này.

## M26 — VFS, tmpfs và FAT32

- PR [#33](https://github.com/duclam2011dz/LamixOS/pull/33) đã merge vào `main`, merge commit `2809e2155c930a061bb6d96ca4df151d085e0481`. Acceptance commit `758f2c9998b8d0ffc85e42f8225835b05d784755`; annotated tag `milestone/M26` object `6a5550fe7473e701f4744625f72a2c1b908291b7` trỏ acceptance commit.
- `LATEST_PROFILE=m26`; M25 và các profile cũ còn chọn được tường minh. M18 vẫn `PLANNED`; M27 đang IN_PROGRESS.
- Profile `m26-kernel` thêm VFS kernel nội bộ, tmpfs root và FAT32 8.3/LFN tại `/data` trên GPT User Data. File API chỉ dành cho kernel/test manager; không thêm syscall, shell/service, hoặc đổi LBP v1. Rename file được hỗ trợ; rename directory trả `VFS_UNSUPPORTED`.
- Codex review tìm hai P1 (ghi LFN vào slot không liền nhau; giải phóng cluster trước khi xóa directory entry) và một P2 (rename directory tmpfs làm stale đường dẫn con). Đã sửa, thêm regression và resolve cả ba review thread. Rename directory tmpfs bị từ chối an toàn.

## Bằng chứng nghiệm thu

- M26 cross-build **PASS**; CTest **4/4 PASS** gồm ASan/UBSan native, VFS TSan, host provenance và guest. SeaBIOS `pc`/OVMF `q35`, 2 vCPU, ba lượt mỗi firmware **6/6 PASS**; mỗi lượt boot hai lần trên cùng ảnh để xác nhận FAT32 persistence và tmpfs volatile.
- Regressions native/host M17, M19–M25 **PASS**; regressions guest M17–M23/M25 **PASS**; M24 guest matrix/fault **27 passed, 1 deselected**. `scripts/smoke.py --reproducible`, validator tài liệu và unittest tài liệu **41/41 PASS**.
- Bốn GitHub workflow **SUCCESS** trên acceptance SHA: Toolchain [41436276928](https://github.com/duclam2011dz/LamixOS/actions/runs/41436276928), Harness [41436276918](https://github.com/duclam2011dz/LamixOS/actions/runs/41436276918), Documentation [41436276993](https://github.com/duclam2011dz/LamixOS/actions/runs/41436276993), Boot Contract [41436276953](https://github.com/duclam2011dz/LamixOS/actions/runs/41436276953).
- Log/provenance build: `build/m26-kernel/build.json`; guest results: `out/m26/results/` and `out/m26/runs/`. Test disk là regular file cô lập. Audit bảo mật độc lập, PC thật và firmware ngoài profile QEMU **NOT RUN**.

## Bước tiếp theo

Tiếp tục implementation M27 theo kế hoạch đã được người dùng duyệt; không bắt đầu M28. Trước khi đóng milestone, kiểm tra lại acceptance đầy đủ, profile `latest`, CI trên acceptance SHA, tag/PR và bàn giao theo quy trình dự án. Nếu sang một phiên lập kế hoạch mới, phải vào Plan Mode thật trước khi khảo sát milestone tiếp theo.

Giữ nguyên file untracked người dùng `PROMPT_NEXT_SESSION_M21.md`; không đưa vào commit.
