# Bàn giao M07 — VERIFYING — 2026-10-29

Trạng thái milestone: M00=DONE; M01=DONE; M02=DONE; M03=DONE; M04=DONE; M05=DONE; M06=DONE

M07 đã có generator/checker GPT image và parity boot SeaBIOS/OVMF dùng cùng một kernel ELF M05. **Chưa đóng M07**: chưa commit/push/PR nên GitHub CI chưa chạy trên SHA chứa thay đổi. M08 chưa bắt đầu.

## Kết quả và phạm vi

- `scripts/m07_image.py`: tạo/check raw sparse 8 GiB theo GPT M03; ESP FAT32 256 MiB có `EFI/BOOT/BOOTX64.EFI` và `EFI/LAMIX/A/KERNEL.ELF`. B/User Data để trống; không mount/ghi device. GUID ngẫu nhiên mặc định, UUIDv5 theo seed tùy chọn.
- `tests/m07/`: host image/GPT/GUID/FAT tests và QEMU boot parity. Cùng image boot thật bằng SeaBIOS và OVMF, cùng kernel hash. Probe phát `M07:LBP`; version, slot A, feature contract/NX hợp lệ ở cả hai. Negative kernel ELF hỏng bị cả hai đường boot từ chối đúng nhóm `ELF`.
- Local: M07 CTest **2/2 PASS**; image create/check PASS, logical 8 GiB, allocated 268,562,432 byte. M05 Debug/Release **4/4** mỗi profile; M06 **3/3**; M03 Debug/Release **4/4** mỗi profile và sanitizer **3/3**; M02 **2 PASS, 2 SKIPPED** (boot skeleton); M04 **2/2**. Host pytest **258 PASS, 2 SKIPPED, 131 deselected**. Validator PASS; unittest **34/34**; doctor profile ci PASS.
- Regression M05/M06/M03/M02/M04 và M07 guest đã dùng QEMU thật với snapshot; OVMF CODE/SeaBIOS firmware hash và protected image extents không đổi, cleanup đạt. PC thật/4Kn/firmware khác: NOT RUN.

## Git và next action

- HEAD lúc bắt đầu: `573cb42f58cc0edebccea82a0719f60cae8660ae`; `main` bằng `origin/main`, worktree ban đầu sạch. M06 tag `milestone/M06` vẫn trỏ acceptance commit `b96c6ba3b0dbb5c60583e40bc83406e1b7eac10f`; M06 DONE.
- Sandbox cho phép sửa workspace nhưng `.git` chỉ đọc; tạo branch thất bại. M07 files đang chưa commit trên checkout `main`. Không tạo/push tag, không có PR/merge.
- CI M07: PENDING/NOT RUN do chưa có commit/PR. **Next exact action:** cấp môi trường có quyền ghi Git metadata, đưa các thay đổi đã review lên branch M07, tạo PR và chờ review/CI xanh trên đúng SHA. Sau đó cập nhật handoff/trạng thái theo bằng chứng và chỉ tag M07 khi người dùng xác nhận hoàn thành riêng.
- Không bắt đầu M08; không tuyên bố image này là OS sản phẩm. Xem [kế hoạch và lịch sử M06](../PLANS.md), [ROADMAP](../ROADMAP.md), [lệnh M07](TESTING.md#image-và-boot-parity-m07).
