# Kế hoạch hiện hành — M08

Trạng thái milestone: M00=DONE; M01=DONE; M02=DONE; M03=DONE; M04=DONE; M05=DONE; M06=DONE; M07=DONE; M08=VERIFYING

Trạng thái M08: **VERIFYING**. Kế hoạch M08 đã được duyệt và triển khai local. QEMU SeaBIOS/OVMF chạy kernel bình thường, `#UD`, guard-page `#PF`; hồi quy local đạt. Chưa có commit/PR hoặc GitHub CI trên SHA chứa M08; chưa đánh dấu DONE.

## Phạm vi triển khai M08

- Thêm `kernel/` với entry NASM, interrupt stubs và kernel C freestanding. Kernel kiểm tra register ABI và gọi `lbp_validate()` trực tiếp; không đổi LBP v1 hay ownership. Tạo GDT/IDT/TSS riêng, selector cũ được giữ hợp lệ trong đoạn chuyển; double fault dùng IST stack trong `.bss`.
- COM1 115200 8N1; panic in vector/error/RIP/CS/RFLAGS/RSP/GPR và CR2 khi #PF. PIC remap, PIT 100 Hz, IRQ0 nhận ba tick/EOI rồi mask và phát `M08:PASS`; chưa có timer API.
- `tests/m08/build_kernel.py` tạo ELF64 ET_EXEC higher-half, `p_paddr=0`, không undefined/dynamic runtime, kèm compiler/linker/commands/source+ELF hashes và inspection. `scripts/m07_image.py` nhận `--kernel-build`; đường mặc định M05 giữ nguyên.
- QEMU tests boot cùng artifact kernel/image qua SeaBIOS và OVMF; fault profile `ud` và `pf` yêu cầu đúng panic vector. Reports nằm dưới `out/m08/`; image/firmware nguồn được hash trước/sau run.

## Kết quả local hiện có

- Kernel build/provenance và host checks: **PASS**, 3/3. M08 guest: **PASS**, 6/6 thật trên BIOS/UEFI (normal, `#UD`, guard `#PF`); panic serial có vector mong đợi, PF có `CR2=guard`.
- Hồi quy: M05 Debug và Release guest/host **PASS**; M06 **3/3 PASS**; M07 **2/2 PASS**; M03 Debug/Release **4/4 PASS**, ASan/UBSan **3/3 PASS**; M04 **2/2 PASS**; M02 **2 PASS, 2 SKIPPED**. Host pytest **264 passed, 2 skipped**.
- Docs validator **PASS**, unittest tài liệu **36/36 PASS**. M01 bootstrap local **NOT RUN**; cần Toolchain CI trên SHA mới.
- GitHub CI trên SHA chứa M08: **PENDING** (chưa commit/push). PC thật và phần cứng PIC/PIT: **NOT RUN**.

## Lệnh chính

```bash
python tests/m08/build_kernel.py --build build/m08
PATH="$PWD/.venv/bin:$PATH" .venv/bin/cmake -S tests/m08 -B build/m08-tests -G Ninja -DPython3_EXECUTABLE="$PWD/.venv/bin/python"
PATH="$PWD/.venv/bin:$PATH" .venv/bin/ctest --test-dir build/m08-tests --no-tests=error --output-on-failure
```

Không đổi BIOS Stage1 gate, Stage2 budget, LBP wire format hoặc kernel entry ABI. M09 chưa bắt đầu; không tạo tag M08 trong lượt này.

---

## Lịch sử M07 — DONE

M07 đã được nghiệm thu, PR #5 merge và tag `milestone/M07` trỏ commit nghiệm thu. Nội dung/kết quả lịch sử xem handoff M07 trong Git trước khi M08 bắt đầu.

---

## Ghi chú lưu trữ M07

## Phạm vi đã triển khai

- Thêm `scripts/m07_image.py` tạo/check raw sparse GPT image 8 GiB; FAT32 ESP 256 MiB chứa `BOOTX64.EFI` và cùng `kernel.elf` M05 tại slot A. System A/B và User Data để trống. GUID mặc định ngẫu nhiên, `--seed` dùng UUIDv5 ổn định.
- Image builder xác minh M05/M06 build reports và source hashes, từ chối đè file, giới hạn output `build/`, `out/`, `/tmp`; checker xác minh PMBR/GPT/CRC/layout, FAT32/BPB, payload paths và hashes.
- Thêm `tests/m07/` host/guest CTest. Một test parity khởi động cùng image qua SeaBIOS và OVMF; negative guest làm hỏng kernel ELF và cả hai đường từ chối với `ELF`. Probe chung phát dòng `M07:LBP` để đối chiếu LBP v1/version, source, slot, required/provided features và record count. Wire ABI không đổi.
- Doctor kiểm tra `mkfs.fat`/mtools; Toolchain CI cài `dosfstools`/`mtools`, chạy M07 CTest và chỉ lưu reports, không lưu image/firmware.

## Kết quả local

- Image CLI create/check: **PASS**; 8 GiB logical, 268,562,432 byte cấp phát trong profile hiện tại; hashes Stage1/Stage2/EFI/kernel khớp artifact. Đây là image probe M07, không phải OS sản phẩm.
- M07 CTest **2/2 PASS**; positive SeaBIOS + OVMF trên cùng image và kernel, negative ELF cả hai firmware; source image extents/firmware và cleanup đạt.
- M05 BIOS Debug/Release **4/4 PASS** mỗi profile. M06 CTest **3/3 PASS**. M03 Debug/Release **4/4 PASS**, sanitizer **3/3 PASS**. M02 CTest **2 PASS, 2 SKIPPED**; M04 **2/2 PASS**.
- Host pytest `tests -m 'not qemu'`: **258 passed, 2 skipped, 131 deselected**. Documentation validator PASS; unittest tài liệu **34/34 PASS**. Doctor profile ci PASS với PATH `.venv/bin`.
- M01 bootstrap/reproducibility local: **NOT RUN** (không đổi inputs/toolchain); CI M01 tương lai phải chạy trên SHA M07 để có bằng chứng SHA đó.

## Git, CI và giới hạn

- PR #5 merge ngày 2026-10-29; merge commit `93724991eadce0699258b22e7a37c209b6f1076b`. Commit nghiệm thu `1ece4235ca6827133081c9e4841e824bf6b05ae2`; commit triển khai ban đầu `ac9d6531540087463fa0a8ec15ad8722c69791f8`. Automated review tìm một race cleanup, đã sửa và thêm regression test; review lại trên SHA nghiệm thu không còn finding.
- Annotated tag `milestone/M07` đã push: tag object `1bbda687ef355962a0d40acc44ffc73547a3a799`, peeled commit `1ece4235ca6827133081c9e4841e824bf6b05ae2`.
- Bốn workflow CI SUCCESS trên SHA nghiệm thu `1ece4235…`: Documentation run `40871303801`, Boot Contract `40871303809`, Harness `40871303839`, Toolchain `40871303879`. Toolchain CI thực thi M05/M06 regression và M07 SeaBIOS/OVMF parity; không thay thế CI của SHA khác.
- PC thật, 4Kn, firmware ngoài profile QEMU, slot B/fallback, filesystem System/User và kernel sản phẩm M08: **NOT RUN/ngoài phạm vi**.

## Lệnh chính

```bash
. .venv/bin/activate
python tests/bios_stage2/build_stage2.py --build build/m07-input --profile Debug
python tests/uefi/build_uefi.py --build build/m06
python scripts/m07_image.py create --image out/m07/lamix.img --bios-build build/m07-input --uefi-build build/m06 --seed local-check
python scripts/m07_image.py check --image out/m07/lamix.img --bios-build build/m07-input --uefi-build build/m06
cmake -S tests/m07 -B build/m07-tests -G Ninja -DPython3_EXECUTABLE="$PWD/.venv/bin/python"
ctest --test-dir build/m07-tests --no-tests=error --output-on-failure
```

Bước tiếp theo M07 khi đóng là M08; hiện M08 đang VERIFYING theo kế hoạch đã duyệt.

---

## Lịch sử M06 — DONE


Người dùng duyệt triển khai M06 ngày 2026-10-28 và xác nhận kết thúc/tag ngày 2026-10-29. M06 đã merge qua PR #4; annotated tag `milestone/M06` trỏ commit nghiệm thu `b96c6ba3b0dbb5c60583e40bc83406e1b7eac10f`. Bốn workflow CI SUCCESS trên SHA nghiệm thu. M07 bắt đầu sau kế hoạch được duyệt riêng; trạng thái và bằng chứng hiện tại ở phần đầu file.

## Contract được giữ

- `LBP v1`, BIOS Stage1 440 byte và Stage2 budget/ABI giữ nguyên. ELF ET_EXEC higher-half, `p_paddr=0`; kernel allocations vật lý dưới 4 GiB. EFI image/trampoline không giả định cùng giới hạn; trampoline/stack/tables được cấp riêng.
- Firmware functions dùng Microsoft x64 ABI; kernel entry dùng System V theo LBP. Trampoline chuyển CR3/stack/registers; LA57 bật bị từ chối.
- ESP xác định từ Loaded Image DeviceHandle, Device Path, Block I/O và GPT; đọc `EFI/LAMIX/A/KERNEL.ELF`, không fallback B.
- LBP v1 hiện giữ nguyên wire layout. UEFI phát framebuffer identity mapping RW/NX khi GOP mode tương thích; ACPI chỉ khi RSDP rev0/2, checksum/span/alignment hợp lệ. OVMF local đưa RSDP không căn 16 nên optional ACPI bị bỏ; required ACPI bị từ chối.
- GetMemoryMap dùng descriptor stride/version/key; 1 MiB buffer ban đầu, một lần tăng bounded nếu BUFFER_TOO_SMALL (tối đa 2 MiB). Không log/cấp phát/protocol sau map thành công trước ExitBootServices; retry Exit tối đa một lần cho key stale. Sau exit không dùng Boot Services.

## Gates và kết quả local

- [x] PE/COFF: EFI subsystem x64, entry, imports rỗng, relocations có; Clang/LLD/NASM version, lệnh và source hashes ghi trong build report.
- [x] QEMU OVMF guest: positive cùng kernel ELF trả `M06:PASS`; negative thiếu kernel, ELF hỏng, ACPI required nhưng không có record hợp lệ, GPT identity lỗi nhận đúng failure.
- [x] LBP/probe: kernel xác nhận feature/record, identity framebuffer mapping và toàn bộ entry mappings; cùng probe BIOS/UEFI.
- [x] M06 CTest: PE, host retry/capacity policy và 5 guest cases PASS; guest positive kiểm marker GOP. BIOS M05 Debug/Release CTest 4/4; M05 host sanitizer 1/1; M03 Debug/Release 4/4, sanitizer 3/3; M02 fixtures/launch 2 PASS, skeleton BIOS/UEFI SKIPPED; M04 host+QEMU 2/2.
- [x] GPT UEFI extent matching, framebuffer mapping above 4 GiB và mutational LBP mapping validation có host tests.
- [x] Review PR #4 phát hiện và đã sửa ba vấn đề: ghi LBP count 64-bit, GOP channel masks, và thiếu build target trước CTest. Bốn CI workflows SUCCESS trên SHA nghiệm thu `b96c6ba3b0dbb5c60583e40bc83406e1b7eac10f`.
- [x] PR #4 merge ngày 2026-10-29; tag object `c5e396f5d2e7a3f8d4d3ccdcbf2eb3c3ea75d9f3` trỏ peeled commit nghiệm thu. PC thật/4Kn chưa kiểm.

## Kiểm thử và rollback

Lệnh chính sau khi activate `.venv`:

```bash
python3 tests/uefi/build_uefi.py --build build/m06
cmake -S tests/uefi -B build/m06-tests -G Ninja
cmake --build build/m06-tests
ctest --test-dir build/m06-tests --no-tests=error --output-on-failure
```

Suite guest tạo image mới trong temporary directory, snapshot disk, CODE read-only, VARS riêng từng run, serial/QMP/deadline/cleanup và hash bảo vệ image/firmware. Báo cáo nằm `out/m06/`; không upload firmware hoặc images. OVMF execution là PASS riêng; OS boot skeleton vẫn SKIPPED vì chưa có image sản phẩm. QEMU không đại diện PC thật.

Nếu cần thay LBP wire layout, kernel ABI, BIOS gate/budget, policy A/B hoặc scope M07/M08 thì dừng và xin duyệt kế hoạch bổ sung. Rollback bằng commit revert; không reset/clean, force-push hoặc di chuyển tag đã push. M07 DONE; M08 chưa bắt đầu.
