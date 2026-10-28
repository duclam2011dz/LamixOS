# M05 — BIOS Stage2 chờ review, 2026-10-28

Trạng thái milestone: M00=DONE; M01=DONE; M02=DONE; M03=DONE; M04=DONE; M05=VERIFYING

Người dùng duyệt triển khai M05. Code/local/CI đã kiểm chứng, đang chờ review và xác nhận hoàn thành riêng; chưa DONE/tag/merge. M06/M07/M08 chưa triển khai. Đây là BIOS→ELF kernel probe, chưa phải kernel sản phẩm hoặc OS hoàn chỉnh.

## Git và phạm vi

- Nhánh feat/m05-bios-stage2, base main/origin `ab2d1e524919f838dce6fbc5412fc70b58274562`.
- Code hiện tại: `0a0f496496380f4bc34ce1536a6000f218cb1a77`, đã push vào [PR #3](https://github.com/duclam2011dz/LamixOS/pull/3), open/chưa merge. Commit đầu 68cbd48 có CI xanh nhưng được sửa tiếp sau review segment bounds, không dùng nó thay nghiệm thu revision mới.
- Annotated M04 object `90778d4b630c9fe5292f5d43c49cd6a91b8670b7` giữ peeled `1c2f001896327d06a3875fa5b7443f847ae2ad7f`. Tags M00–M04 không đổi.
- Bản handoff thuộc commit tài liệu tiếp sau code. Lấy HEAD thật bằng git rev-parse HEAD và [PR checks](https://github.com/duclam2011dz/LamixOS/pull/3/checks), không tạo vòng lặp ghi SHA của chính handoff. CI code dưới đây không thay CI HEAD tài liệu mới.

Stage2 bootstrap32 KiB/core C32, BIOS thunk/E820/A20, GPT/FAT32/ELF parser và allocator callback, loader reservations/page tables/LBP, probe64. Stage1 source/hash/440 byte và LBP/GPT M03 giữ nguyên. Xem [audit](BIOS_STAGE2.md), [kế hoạch](../PLANS.md), [lệnh tests](TESTING.md#bios-stage2-m05). Formatter sparse8 GiB chỉ test-only, không mount/device/overwrite/generator sản phẩm.

## Bằng chứng local

| Nhóm | Kết quả |
|---|---|
| Compile/link32/64 và Stage1 | PASS assertions/alignment, không runtime host/libgcc64/undefined; Stage1 SHA256 a63736f43692aa428de888e6fd10979f098be29084fa79a8e4ef7ca1449296c4 |
| Budget | PASS bootstrap32768; core Debug44528 file/176128 memory, Release37128/172032; dưới256 KiB |
| M05 Debug/Release | PASS CTest4/4 mỗi profile; host46/guest54 (9 positives,45 expected PANIC) |
| ASan/UBSan native | PASS CTest1/1, parsers/division/load/BSS/mutations |
| Global host | PASS252,125 qemu deselected |
| Tài liệu | PASS validator/33 unittest |
| M03 | PASS CTest4/4, sanitizer3/3, cross compile-only |
| M04 | PASS suites2/2, gồm69 guest cases |
| M02 | PASS33 fixtures,2 BIOS/UEFI paused launches;2 OS skeleton SKIPPED/77 |
| M01 | Local không chạy lại; Toolchain CI đã chạy reproducibility PASS |

Bản sửa real-mode dùng segment:offset chuẩn cho CRC/BSS/E820, tránh phụ thuộc hidden limits BIOS. Adapter inject-segments đặt lại limits64 KiB trước/sau BIOS calls; large-core zero-extend file đến70000 byte (memory budget không đổi), CRC corruption byte cuối phải fail. RO/guard nhận #PF vector0e; EIO thật ở core/FAT/kernel payload nhận DISK. Test adapters E820/A20/LBP/map có nhãn riêng, không coi là firmware thật tạo lỗi đó.

Logs/reports ignored out/m05: XML, serial/QMP/result/case/layout JSON; compiler logs/listings/maps/build.json trong build/m05 và build/m05-release. Giữ guest failure logs cũ; không dùng failed runs làm PASS. Gate-only ELF không dùng trong image. Negative guests giữ PANIC, pytest PASS nghĩa khớp expected. Source/artifact hash guard chặn binary cũ sau source changes; fixture output và compiler adapter chỉ dùng file mới.

## CI trên code revision đã sửa

Bốn workflows SUCCESS trên đúng `0a0f496496380f4bc34ce1536a6000f218cb1a77`:

- [Documentation CI](https://github.com/duclam2011dz/LamixOS/actions/runs/40692182135).
- [Boot Contract CI](https://github.com/duclam2011dz/LamixOS/actions/runs/40692182246).
- [Harness CI](https://github.com/duclam2011dz/LamixOS/actions/runs/40692182117).
- [Toolchain CI](https://github.com/duclam2011dz/LamixOS/actions/runs/40692182030).

Đã đọc ZIP m05-target-reports id12818215770: host46/guest54 mỗi profile,108 case.json/result.json (18 PASS/90 PANIC đúng expected), strict-segment/cross-segment đạt; hashes source/binary Debug/Release khớp local, protected hashes/firmware/cleanup đúng. ZIP không có binary/image/firmware. Audit: out/m05/ci-artifact-audit-0a0f496.json. Host sanitizer thuộc Boot Contract CI; không dùng artifact68cbd48 thay revision mới.

## Giới hạn và next exact action

Xác minh CI HEAD tài liệu cuối sau push, rồi chờ người dùng review PR #3/xác nhận hoàn thành riêng trước DONE/merge/tag. Không còn code đang dở được biết. Không tự bắt đầu M06, tạo tag M05/version tag hoặc Release.

OS boot skeleton BIOS/UEFI vẫn SKIPPED vì chưa có image sản phẩm mặc định. UEFI M06/parity M07/kernel M08, PC thật/4Kn/EDD không hỗ trợ/firmware vi phạm ABI, GDB và NMI/SMM stress NOT RUN. Không suy ra các kết quả đó từ kernel probe/launch-only.
