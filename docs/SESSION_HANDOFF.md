# Bàn giao M08 — DONE — 2026-10-29

Trạng thái milestone: M00=DONE; M01=DONE; M02=DONE; M03=DONE; M04=DONE; M05=DONE; M06=DONE; M07=DONE; M08=DONE

## Kết quả

M08 đã được nghiệm thu, merge và gắn annotated tag. Kernel freestanding C/NASM dùng chung một ELF chạy thật qua SeaBIOS và OVMF, xác thực LBP v1, thiết lập GDT/IDT/TSS cùng IST double-fault, xuất panic context qua COM1, và kiểm tra IRQ0/PIT ba tick. BIOS Stage1 gate/budget và LBP wire format không đổi.

- PR [#6](https://github.com/duclam2011dz/LamixOS/pull/6) merged bằng merge commit `6b794a6c454dfab6cdcc79abf26d8f3844d87d64`.
- Commit nghiệm thu: `56cbc3bd290d6b76789a50a8c80762896b57f4d4` (`fix(m08): map optional ACPI span for validation`). Annotated tag `milestone/M08` tag object `ba5642cf9c2ae615c5b2687f6d7602b42a54fe54`, peeled commit trùng SHA nghiệm thu. Tag được đẩy lên `origin` và không trỏ merge/closure commit.
- `origin/main` sau merge M08: `6b794a6c454dfab6cdcc79abf26d8f3844d87d64`. Handoff này được ghi trong commit đóng thủ tục; không ghi SHA của chính commit chứa handoff.

## Kiểm chứng

- M08 CTest **PASS 2/2**; guest boot **PASS 6/6**: BIOS/UEFI × normal, `#UD` vector 6, guard-page `#PF` vector 14. Normal nhận ít nhất ba PIT ticks và phát `M08:PASS`; panic profile phát `M08:PANIC`, page fault có CR2 khớp guard. Image extents, firmware và artifacts giữ nguyên; QEMU cleanup sạch.
- Sau sửa review ACPI: host pytest **266 passed, 137 deselected**; docs validator **PASS**; unittest tài liệu **PASS 36/36**. Các guest cases được chạy riêng qua M08 CTest.
- Regression local được ghi trong kế hoạch: M05 Debug/Release, M06 **3/3**, M07 **2/2**, M03 Debug/Release **4/4** và ASan/UBSan **3/3**, M04 **2/2** đều PASS; M02 **2 PASS, 2 SKIPPED**.
- Bốn GitHub workflows **SUCCESS trên đúng SHA nghiệm thu**: [Documentation CI](https://github.com/duclam2011dz/LamixOS/actions/runs/40882457559), [Harness CI](https://github.com/duclam2011dz/LamixOS/actions/runs/40882457555), [Toolchain CI](https://github.com/duclam2011dz/LamixOS/actions/runs/40882457670), [Boot Contract CI](https://github.com/duclam2011dz/LamixOS/actions/runs/40882457776).
- Review tìm thấy kernel chưa truyền span ACPI cho `lbp_validate()`. Commit `56cbc3b` thêm scratch mapping RSDP đọc-only, truyền span rồi gỡ mapping; thread đã được phản hồi và resolve. OVMF trong profile hiện tại bỏ RSDP không căn 16 byte, nên guest path có ACPI hợp lệ **NOT RUN**; wiring được kiểm bằng host structural test và validator span regression M03.

## Giới hạn và tiếp tục

- PC thật, firmware ngoài profile OVMF, PIC/PIT trên phần cứng, M01 bootstrap local: **NOT RUN**. Toolchain CI đã chạy pipeline/bootstrap theo cấu hình CI.
- M08 là kernel bootstrap/probe tối thiểu, chưa phải kernel sản phẩm hoặc Lamix OS hoàn chỉnh. Chưa có PMM/VMM/heap/scheduler.
- M09 vẫn **PLANNED**, chưa bắt đầu. Next exact action: chờ yêu cầu riêng để vào Plan Mode và lập kế hoạch M09; không tự triển khai.

Chi tiết lệnh và kiểm thử ở [TESTING](TESTING.md#kernel-tối-thiểu-m08), kế hoạch và bằng chứng milestone ở [PLANS](../PLANS.md), mục tiêu/thứ tự ở [ROADMAP](../ROADMAP.md).
