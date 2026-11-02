# Bàn giao hiện hành — M26 DONE — 2026-11-02

Trạng thái milestone: M00=DONE; M01=DONE; M02=DONE; M03=DONE; M04=DONE; M05=DONE; M06=DONE; M07=DONE; M08=DONE; M09=DONE; M10=DONE; M11=DONE; M12=DONE; M13=DONE; M14=DONE; M15=DONE
Trạng thái hiện hành: M16 DONE; M17 DONE; M18 PLANNED; M19 DONE; M20 DONE; M21 DONE; M22 DONE; M23 DONE; M24 DONE; M25 DONE; M26 DONE; M27–M44 PLANNED

## M26 — VFS, tmpfs và FAT32

- PR [#33](https://github.com/duclam2011dz/LamixOS/pull/33) đã merge vào `main`, merge commit `2809e2155c930a061bb6d96ca4df151d085e0481`. Acceptance commit `758f2c9998b8d0ffc85e42f8225835b05d784755`; annotated tag `milestone/M26` object `6a5550fe7473e701f4744625f72a2c1b908291b7` trỏ acceptance commit.
- `LATEST_PROFILE=m26`; M25 và các profile cũ còn chọn được tường minh. M18 vẫn `PLANNED`; M27 chưa bắt đầu.
- Profile `m26-kernel` thêm VFS kernel nội bộ, tmpfs root và FAT32 8.3/LFN tại `/data` trên GPT User Data. File API chỉ dành cho kernel/test manager; không thêm syscall, shell/service, hoặc đổi LBP v1. Rename file được hỗ trợ; rename directory trả `VFS_UNSUPPORTED`.
- Codex review tìm hai P1 (ghi LFN vào slot không liền nhau; giải phóng cluster trước khi xóa directory entry) và một P2 (rename directory tmpfs làm stale đường dẫn con). Đã sửa, thêm regression và resolve cả ba review thread. Rename directory tmpfs bị từ chối an toàn.

## Bằng chứng nghiệm thu

- M26 cross-build **PASS**; CTest **4/4 PASS** gồm ASan/UBSan native, VFS TSan, host provenance và guest. SeaBIOS `pc`/OVMF `q35`, 2 vCPU, ba lượt mỗi firmware **6/6 PASS**; mỗi lượt boot hai lần trên cùng ảnh để xác nhận FAT32 persistence và tmpfs volatile.
- Regressions native/host M17, M19–M25 **PASS**; regressions guest M17–M23/M25 **PASS**; M24 guest matrix/fault **27 passed, 1 deselected**. `scripts/smoke.py --reproducible`, validator tài liệu và unittest tài liệu **41/41 PASS**.
- Bốn GitHub workflow **SUCCESS** trên acceptance SHA: Toolchain [41436276928](https://github.com/duclam2011dz/LamixOS/actions/runs/41436276928), Harness [41436276918](https://github.com/duclam2011dz/LamixOS/actions/runs/41436276918), Documentation [41436276993](https://github.com/duclam2011dz/LamixOS/actions/runs/41436276993), Boot Contract [41436276953](https://github.com/duclam2011dz/LamixOS/actions/runs/41436276953).
- Log/provenance build: `build/m26-kernel/build.json`; guest results: `out/m26/results/` and `out/m26/runs/`. Test disk là regular file cô lập. Audit bảo mật độc lập, PC thật và firmware ngoài profile QEMU **NOT RUN**.

## Bước tiếp theo

Phiên sau chỉ lập kế hoạch M27 theo `ROADMAP.md` trong Codex Plan Mode thật. Người dùng phải chủ động bật Plan Mode trên giao diện. Đọc `AGENTS.md`, handoff này, `PLANS.md` và mục M27 trong `ROADMAP.md`; xác minh Git/remote/tag/CI hiện hành. Chỉ khảo sát và lập kế hoạch, không sửa code/tài liệu hoặc bắt đầu triển khai; kết thúc bằng đề xuất và chờ duyệt rõ ràng. Prompt mở phiên có sẵn tại [`PROMPT_NEXT_SESSION_M27.md`](../PROMPT_NEXT_SESSION_M27.md).

Giữ nguyên file untracked người dùng `PROMPT_NEXT_SESSION_M21.md`; không đưa vào commit.
