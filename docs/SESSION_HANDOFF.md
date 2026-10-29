# Bàn giao M08 — VERIFYING — 2026-10-29

Trạng thái milestone: M00=DONE; M01=DONE; M02=DONE; M03=DONE; M04=DONE; M05=DONE; M06=DONE; M07=DONE; M08=VERIFYING

## Trạng thái

M08 đã được duyệt triển khai. Local implementation và QEMU boot thực đã chạy; chưa commit/PR/push, vì vậy GitHub CI trên SHA chứa M08 còn **PENDING**. Chưa tạo tag `milestone/M08`; M09 chưa bắt đầu.

## Thay đổi

- `kernel/`: freestanding C/NASM ELF64, LBP v1 validation, GDT/IDT/TSS, double-fault IST, COM1 panic context, PIC remap và PIT IRQ0 three-tick self-test.
- `tests/m08/`: build report/provenance và host/guest CTest. Fault images `#UD`/guard `#PF` phát panic đúng vector; positive phát `M08:PASS`.
- `scripts/m07_image.py`: tùy chọn `--kernel-build` cho create/check; mặc định M05 và API `--kernel` cũ vẫn giữ.
- Toolchain CI có job M08 guest và report paths.

## Kết quả local

- M08 host/provenance **PASS 3/3**; BIOS/UEFI guest **PASS 6/6** (normal, vector 6, vector 14). Logs ghi `M08:LBP`, marker/fault đúng; page fault có `CR2` bằng guard. QEMU OVMF dùng CODE read-only, VARS bản riêng; QEMU snapshot và cleanup được harness quản lý. Hash protected extents, firmware và boot artifacts không đổi trước/sau run.
- Regression: M05 Debug/Release host+guest **PASS**; M06 **3/3 PASS**; M07 **2/2 PASS**; M03 Debug/Release **4/4 PASS**, sanitizer **3/3 PASS**; M04 **2/2 PASS**; M02 **2 PASS, 2 SKIPPED**; pytest host **264 PASS, 2 SKIPPED**.
- M05 Release và M03 host suite lần đầu dùng build dirs stale; provenance/mismatch phát hiện trước guest. Rebuild từ source hiện hành và chạy lại thành công.
- Docs validator **PASS**, unittest tài liệu **36/36 PASS**. M01 bootstrap local **NOT RUN**. PC thật/PIC/PIT trên phần cứng **NOT RUN**. GitHub CI **PENDING** do chưa commit/push.

## Git và next action

- Checkout lúc bắt đầu: `main` tại `27ac1fb3d676bb1a6fade16b6a6a6371987cfd6b`, sạch và tracking `origin/main`; remote `origin` trỏ repo chính thức. M08 hiện là diff chưa commit.
- Next exact action: hoàn tất docs validation/diff review, sau đó giao người dùng review và tạo PR theo quy trình. Chỉ báo M08 DONE sau review/CI xanh trên SHA nghiệm thu và xác nhận hoàn thành riêng; chỉ tạo/push annotated tag sau xác nhận đó.

Chi tiết lệnh và test ở [TESTING](TESTING.md#kernel-tối-thiểu-m08), kế hoạch/hồi quy ở [PLANS](../PLANS.md), mục tiêu roadmap ở [ROADMAP](../ROADMAP.md).
