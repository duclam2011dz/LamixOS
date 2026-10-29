# Bàn giao M09 — VERIFYING — 2026-10-29

Trạng thái milestone: M00=DONE; M01=DONE; M02=DONE; M03=DONE; M04=DONE; M05=DONE; M06=DONE; M07=DONE; M08=DONE; M09=VERIFYING

## Trạng thái Git và phạm vi

- Workspace bắt đầu sạch trên `main` tại `3f7a66284a51046baef5bbf0ff18007b8dde54b4`; thay đổi M09 hiện ở nhánh local `codex/m09-bitmap-pmm` trên cùng base SHA. Chưa commit, push, PR, merge hoặc tạo tag M09.
- M09 thêm PMM bitmap 2-bit/frame dưới 4 GiB; chỉ `LBP_USABLE`, reservation bị loại, không reclaim LOADER/ACPI, không zero frame. LBP v1, loader và BIOS gates không đổi.
- M09 chưa được nghiệm thu cuối. M10 chưa bắt đầu. GitHub CI trên SHA M09 **PENDING**.

## Kiểm chứng local

- Kernel M09 build **PASS**; GCC cross 12.2.0, GNU Binutils 2.39, NASM 2.15.05. Build report gồm commands, versions, source/artifact hashes và ELF inspection.
- M09 CTest **PASS 3/3**: host pytest, native PMM ASan/UBSan, guest BIOS/UEFI. QEMU guest chạy thật cùng ELF SHA-256 `dafa8503e845ea8b59e46591dac0ed01ccdd78b3a50ac2fcd8c54662bbfe7f98`; SeaBIOS và OVMF đều phát `M08:PASS` rồi `M09:PASS`.
- Host coverage kiểm usable extents/holes, giới hạn 4 GiB, map/storage lỗi, OOM, invalid/double-free, reservation và stress. Guest tự cấp tám frame khác nhau, xác minh thuộc usable, thử free sai/lặp và khôi phục free count.
- Báo cáo guest: `out/m09/results/bios.json`, `out/m09/results/uefi.json`; free frames sau test lần lượt `0xfd01` và `0xd1d5` (memory map khác nhau theo firmware). Cleanup không bị cưỡng bức; image extents, firmware và build inputs có hash trước/sau giống nhau.

## Regression và giới hạn

- Regression local: M02 harness **2 PASS/2 SKIPPED**; M03 Debug/Release **4/4** mỗi cấu hình, sanitizer **3/3**, cross compile-only **PASS**; M04 **2/2**; M05 Debug/Release **4/4** mỗi cấu hình và sanitizer **1/1**; M06 **3/3**; M07 **2/2**; M08 **2/2**. M02 skips là boot skeleton chưa có image sản phẩm.
- Toàn host pytest `tests -m 'not qemu'`: **272 passed, 139 deselected**. Docs validator **PASS**, unittest tài liệu **37/37 PASS**, `git diff --check` **PASS**. M01 bootstrap local **NOT RUN**; thay đổi không chạm toolchain inputs.
- PC thật, RAM trên 4 GiB, thu hồi LOADER/ACPI reclaim và firmware ngoài profile QEMU **NOT RUN/ngoài phạm vi**. M09 chưa phải PMM có mapping, zeroing, contiguous allocation hoặc đồng bộ SMP; kernel vẫn là bootstrap/probe, không phải OS sản phẩm.
- Bốn workflow CI/PR chưa chạy trên nội dung M09; không suy kết quả từ SHA M08.

## Bước tiếp theo

Các kiểm chứng local trong phạm vi đã hoàn tất. Bước kế tiếp là review thay đổi, rồi theo yêu cầu riêng của người dùng mới commit/PR và chạy CI trên SHA nghiệm thu. Chỉ đánh dấu DONE và tạo/push `milestone/M09` khi có nghiệm thu và xác nhận riêng. Không bắt đầu M10 trong thủ tục này.

Chi tiết lệnh tại [TESTING](TESTING.md#bitmap-pmm-m09), phạm vi tại [PLANS](../PLANS.md), mục tiêu tại [ROADMAP](../ROADMAP.md).
