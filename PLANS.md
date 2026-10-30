# Kế hoạch hiện hành — M12

Trạng thái milestone: M00=DONE; M01=DONE; M02=DONE; M03=DONE; M04=DONE; M05=DONE; M06=DONE; M07=DONE; M08=DONE; M09=DONE; M10=DONE; M11=DONE; M12=VERIFYING

Trạng thái M12: **VERIFYING (2026-10-30)** trên nhánh `codex/m12-buddy-pmm`; PR #10 đang mở. Trên head `214229507800f1c41ce34f8c55c3fd42662454c7`, Documentation, Harness và Boot Contract CI SUCCESS; Toolchain CI FAILURE ở smoke/CTest M01 trước khi các bước M12 chạy. Bitmap M09 giữ mặc định; Buddy là backend thử nghiệm trong profile `m12-kernel`, VMM/heap chỉ dùng Buddy ở profile thử nghiệm. Không đổi LBP v1, kernel entry ABI, loaders, BIOS Stage1 gate hoặc Stage2 budget.

## Phạm vi M12 — Buddy allocator thử nghiệm song song

- `kernel/pmm.c`/`kernel/pmm.h` thêm tree bitmap 2-bit/node cho frames dưới 4 GiB, boundary-aware coalescing và order 0–18; API bitmap M09 không đổi. Chỉ frame đầy đủ thuộc `LBP_USABLE` được đưa vào pool; reservations/hole/extent edges ngăn coalesce. Không reclaim LOADER/ACPI và không cấp RAM trên 4 GiB.
- Kernel profile `m12-kernel` dành 512 KiB cho node state và 64 KiB cho boundary capacity trong `.bss`; tổng reserve metadata **589,824 bytes**. Khởi tạo Buddy chỉ sau validation map/reservations. Profile M12 route VMM/heap callbacks qua wrapper order 0; các profile trước giữ bitmap.
- Tests trong `tests/m12/` dùng cùng `kernel/pmm.c` trên native ASan/UBSan, host provenance, deterministic benchmark với bitmap contiguous adapter, workload fragmentation/pressure/OOM và QEMU guest. Host benchmark không đại diện hiệu năng guest.
- Rollback bằng profile/backend bitmap; bitmap vẫn là backend mặc định. Không chuyển mặc định theo kết quả hiện tại: mixed-order host trace có throughput thấp hơn ở Buddy; percentile thay đổi theo workload; fragmentation fixture cho kết quả giống nhau.

## Kết quả xác minh local M12

- Build cross profile `m12-kernel` **PASS**; ELF SHA-256 hiện tại ghi trong `build/m12/build.json` (artifact cục bộ, không commit).
- M12 CTest **6/6 PASS**: Buddy ASan/UBSan, host provenance/image, benchmark native và BIOS/OVMF guest. Cùng kernel boot hai firmware; serial phát M08–M12 PASS; hashes protected inputs giữ nguyên, cleanup không cưỡng bức.
- M11 CTest **3/3 PASS**. M07 2/2, M08 2/2, M10 3/3 regressions PASS; M09 sanitizer PASS, host/guest setup sandbox-blocked do wrapper ghi dưới `build/`. PC thật/firmware ngoài QEMU **NOT RUN**.
- Benchmark host seed `0x12a110c`, trace hash `3ded994161041ff7`, 20,000 trace operations × 5 repetitions. Chỉ latency/throughput của allocator calls được tính; no-op bị bỏ qua và báo riêng. Lần chạy native kiểm tra sau hiệu chỉnh ghi nhận 50,385 allocator calls và 49,615 no-op mỗi backend, 50,385 latency samples; p50/p95/max bitmap 130/385/23,723 ns, Buddy 128/1,015/53,576 ns; throughput 6,762,287/2,979,290 allocator calls/s; mean init 768,026/7,574,075 ns. Metadata reserve bitmap/Buddy 262,144/589,824 B. Các số host dao động theo tải máy và chỉ mô tả lần chạy này. Fixture alternate-free: cả hai còn 2,048 frames, largest block 4 KiB, order 1 thất bại; chưa thấy fragmentation advantage trong workload này. Không suy ra hiệu năng guest.
- Documentation validator PASS, unittest 40/40 PASS; `git diff --check` PASS. Toolchain CI run [41082791636](https://github.com/duclam2011dz/LamixOS/actions/runs/41082791636) **FAILURE** hai lần tại smoke/CTest M01; downstream M12 CI step SKIPPED. Artifact log chi tiết chưa truy cập được. Thay đổi benchmark hiện tại chưa push, CI chưa kiểm tra.

## Lệnh M12

```bash
python3 tests/m12/build_kernel.py --build build/m12
cmake -S tests/m12 -B build/m12-tests -G Ninja -DPython3_EXECUTABLE="$PWD/.venv/bin/python"
cmake --build build/m12-tests
ctest --test-dir build/m12-tests --no-tests=error --output-on-failure
```

Trạng thái M11: **DONE (2026-10-30)**. Commit nghiệm thu `3bf87334999a794bce6dec7e8d691f5f125ff781`; PR [#9](https://github.com/duclam2011dz/LamixOS/pull/9) merge commit `c1d288e7ca4fcc9f87c9eb08ba257098e6111bc8`. Bốn GitHub Actions workflows SUCCESS trên commit nghiệm thu. Annotated tag `milestone/M11` object `618d9a27610e5ce73c81abad120b04872f60587b` trỏ commit nghiệm thu. Commit bàn giao tài liệu được tạo sau tag; tag không di chuyển.

## Phạm vi triển khai M11 — Kernel heap và nền tảng C/C++

- Thêm `kernel/heap.c`/`kernel/heap.h`: heap BSP-only trong aperture 1 GiB từ `0xffffe00000000000`, page backing theo nhu cầu qua PMM/VMM. Free-list first-fit có chia/gộp; mapped pages được giữ lại để tái sử dụng.
- C API cung cấp init, alloc, calloc zero, free và stats. Căn 16 byte; OOM trả `NULL`; zero-size trả `NULL`; calloc kiểm tra overflow; invalid pointer và double-free có status riêng. Không thêm global `new/delete`, đồng bộ SMP hoặc IRQ safety.
- Kernel self-test gọi C++ freestanding smoke consumer; profile `m11-kernel` ghi provenance, compiler/linker, source/artifact hashes và ELF inspection. Image tool giữ M05 default và profile trước đó.
- Toolchain CI được mở rộng để chạy M11 CTest và lưu reports, không lưu image/firmware.

## Kết quả xác minh local M11

- M11 CTest **3/3 PASS**: heap implementation dưới ASan/UBSan, host provenance/C++ cross smoke, và BIOS/OVMF guest. Cùng kernel ELF SHA-256 `9500f6c599c96a0430224cb86a9903412735802982308ed9a0c3fa9b7939b4a5`; cả hai firmware phát `M08:PASS`, `M09:PASS`, `M10:PASS`, `M11:PASS`.
- Hồi quy M07 **2/2**, M08 **2/2**, M09 **3/3**, M10 **3/3 PASS**. Guest QEMU dùng cùng image, snapshot/firmware VARS riêng, protected hashes trước/sau và cleanup không cưỡng bức.
- Build provenance: GCC/G++ cross 12.2.0, GNU LD 2.39, NASM 2.15.05; ELF ET_EXEC x86_64 có 3 load segments, không undefined symbols; C++ smoke object liên kết vào ELF freestanding.
- Documentation validator **PASS**, unittest tài liệu **39/39 PASS**, `git diff --check` **PASS**. Bốn workflow CI SUCCESS trên SHA nghiệm thu. PC thật và firmware ngoài profile QEMU: **NOT RUN**.

## Lệnh M11

```bash
python tests/m11/build_kernel.py --build build/m11
PATH="$PWD/.venv/bin:$PATH" .venv/bin/cmake -S tests/m11 -B build/m11-tests -G Ninja -DPython3_EXECUTABLE="$PWD/.venv/bin/python"
PATH="$PWD/.venv/bin:$PATH" .venv/bin/cmake --build build/m11-tests
PATH="$PWD/.venv/bin:$PATH" .venv/bin/ctest --test-dir build/m11-tests --no-tests=error --output-on-failure
```

## Kế hoạch M10 đã nghiệm thu

Trạng thái M10: **DONE (2026-10-30)**. PR #8 merge commit `493cd8a8e6790d0d1adbd87e7500b596698613dd`; head nghiệm thu `7e0b2f288e0497deb4a2c5fd4a50c679643643ba`. Bốn GitHub Actions workflows SUCCESS trên head này. Review phát hiện và sửa quyền execute khi NX không khả dụng tại `7e0b2f2`; M10 CTest sau sửa 3/3 PASS. Tag `milestone/M10` giữ nguyên, trỏ commit ban đầu `dd6365c6c217bfc076f73ec0cb5efdcb8e05a777`; không di chuyển tag công khai.

## Phạm vi triển khai M10 — VMM và kernel address space

- Thêm `kernel/vmm.c`/`kernel/vmm.h`: BSP paging 4 cấp, root CR3 mới, map/unmap/protect trang 4 KiB và range, query, TLB invalidation, W^X và NX khi LBP cung cấp feature.
- Dựng lại mapping kernel segments, BootInfo identity RO, stack identity RW, guard không map và framebuffer uncached. Page-table frames do PMM cấp, truy cập qua scratch data/control aliases; không tạo direct map. Caller giữ ownership leaf frame; VMM thu hồi table frames rỗng. TABLES cũ vẫn unavailable.
- Thêm M10 build provenance/profile và hỗ trợ `m10-kernel` trong image checker; mặc định kernel M05 cùng profile M08/M09 giữ nguyên. Host simulator/sanitizer và guest fault profiles RO write/NX execute/guard.
- Không đổi LBP v1, entry ABI, BIOS Stage1 gate hoặc Stage2 budget; chưa có process address spaces, SMP, heap hay reservation reclamation.

## Kết quả xác minh local M10

- M10 CTest **3/3 PASS**: native VMM ASan/UBSan, host build/image checks và guest suite; guest **8/8 PASS** trên cùng kernel profiles BIOS/OVMF. Normal phát `M08:PASS`, `M09:PASS`, `M10:PASS`; RO/NX/guard faults có vector 0e, error bits và CR2 mong đợi.
- Root CR3 mới giữ mapping cần thiết; allocator scratch cũ được sửa để không tái sử dụng table page còn được root tham chiếu. Image extents, firmware và kernel hashes không đổi; cleanup guest không bị cưỡng bức.
- Docs validator **PASS**, unittest tài liệu **38/38 PASS**; hồi quy M07 CTest **2/2**, M08 **2/2**, M09 **3/3 PASS**. M10 CTest sau review **3/3 PASS**, guest **8/8 PASS**.
- CI bốn workflow SUCCESS trên SHA `7e0b2f288e0497deb4a2c5fd4a50c679643643ba`: [Toolchain](https://github.com/duclam2011dz/LamixOS/actions/runs/40902948477), [Harness](https://github.com/duclam2011dz/LamixOS/actions/runs/40902948404), [Documentation](https://github.com/duclam2011dz/LamixOS/actions/runs/40902948422), [Boot Contract](https://github.com/duclam2011dz/LamixOS/actions/runs/40902948480).
- PR [#8](https://github.com/duclam2011dz/LamixOS/pull/8) đã merge. Annotated tag `milestone/M10` trỏ commit `dd6365c6c217bfc076f73ec0cb5efdcb8e05a777`; review-fix commit `7e0b2f288e0497deb4a2c5fd4a50c679643643ba` được merge sau đó. Tag không bị di chuyển.

## Lệnh M10

```bash
python tests/m10/build_kernel.py --build build/m10
PATH="$PWD/.venv/bin:$PATH" .venv/bin/cmake -S tests/m10 -B build/m10-tests -G Ninja -DPython3_EXECUTABLE="$PWD/.venv/bin/python"
PATH="$PWD/.venv/bin:$PATH" .venv/bin/cmake --build build/m10-tests
PATH="$PWD/.venv/bin:$PATH" .venv/bin/ctest --test-dir build/m10-tests --no-tests=error --output-on-failure
```

## Lịch sử M09 — Bitmap PMM

## Phạm vi M09 — Bitmap PMM

- Thêm `kernel/pmm.c`/`kernel/pmm.h`: bitmap 2-bit cho 1,048,576 frame dưới 4 GiB; chỉ `LBP_USABLE` đầy đủ 4 KiB pages có thể cấp. Reservations được overlay thành unavailable. Không reclaim LOADER/ACPI, không zero, không cấp phát contiguous và không đổi LBP/loader ABI.
- `pmm_init()` nhận view đã được `lbp_validate()` kiểm tra; validate memory/reservation table shape trước khi sửa bitmap. `pmm_alloc_frame()` dùng next-fit scan, báo OOM; `pmm_free_frame()` từ chối unaligned, ngoài giới hạn, chưa cấp hoặc double-free.
- Kernel profile M09 tự khởi tạo PMM, cấp tám frame, kiểm tra usable/reservations/uniqueness, thử invalid/double-free, trả frame và xác minh free count khôi phục; phát `M09:PASS`. M08 marker và IRQ self-test vẫn chạy trước đó.
- M09 build wrapper tạo provenance `m09-kernel`; `scripts/m07_image.py` chấp nhận M09 report khi chỉ định `kernel_build`, giữ mặc định M05 và profile M08.

## Kết quả local hiện có

- M09 CTest: **3/3 PASS** — host pytest, cùng mã PMM dưới ASan/UBSan, và BIOS/OVMF guest. Hai firmware boot cùng kernel ELF SHA-256 `dafa8503e845ea8b59e46591dac0ed01ccdd78b3a50ac2fcd8c54662bbfe7f98`; cả hai phát `M08:PASS` và `M09:PASS`. Free count được khôi phục thành `0xfd01` (BIOS) và `0xd1d5` (OVMF); memory map/reservations khác nhau theo firmware.
- Guest QEMU cleanup không bị cưỡng bức; image extents, BIOS/OVMF firmware, loader inputs và kernel được hash trước/sau, không đổi. Reports tại `out/m09/results/` (local, không commit/upload).
- Build report xác minh compiler/linker, source hashes, ELF inspection và artifact hash. Host tests kiểm multiple extents/holes, giới hạn 4 GiB, storage/map lỗi, OOM, free lỗi và stress; sanitizer chạy implementation PMM thật.
- Regression local: M02 harness 2 PASS/2 SKIPPED; M03 Debug/Release 4/4 mỗi cấu hình, sanitizer 3/3 và cross compile-only PASS; M04 **2/2**, M05 Debug/Release **4/4** mỗi cấu hình cùng sanitizer **1/1**, M06 **3/3**, M07 **2/2**, M08 **2/2** PASS. Host pytest `tests -m 'not qemu'`: **272 passed, 139 deselected**; docs validator PASS; unittest tài liệu **37/37 PASS**; `git diff --check` PASS.
- Bốn workflow GitHub Actions SUCCESS trên SHA nghiệm thu `b2ef8182e75fcf9665374fd20881ed0bfc3fbb8d`: [Toolchain CI](https://github.com/duclam2011dz/LamixOS/actions/runs/40892095834), [Documentation CI](https://github.com/duclam2011dz/LamixOS/actions/runs/40892095745), [Harness CI](https://github.com/duclam2011dz/LamixOS/actions/runs/40892096005), [Boot Contract CI](https://github.com/duclam2011dz/LamixOS/actions/runs/40892095752). M01 bootstrap local **NOT RUN**; Toolchain CI chạy bootstrap và regression theo workflow.

## Lệnh M09

```bash
python tests/m09/build_kernel.py --build build/m09
PATH="$PWD/.venv/bin:$PATH" .venv/bin/cmake -S tests/m09 -B build/m09-tests -G Ninja -DPython3_EXECUTABLE="$PWD/.venv/bin/python"
PATH="$PWD/.venv/bin:$PATH" .venv/bin/cmake --build build/m09-tests
PATH="$PWD/.venv/bin:$PATH" .venv/bin/ctest --test-dir build/m09-tests --no-tests=error --output-on-failure
```

Không đổi LBP v1, loader, BIOS Stage1 gate hoặc Stage2 budget. M09 đã được người dùng duyệt kết thúc và tag; M10 vẫn PLANNED, cần một yêu cầu/lượt kế hoạch riêng.

## Lịch sử M08 — DONE

- Thêm `kernel/` với entry NASM, interrupt stubs và kernel C freestanding. Kernel kiểm tra register ABI và gọi `lbp_validate()` trực tiếp; không đổi LBP v1 hay ownership. Tạo GDT/IDT/TSS riêng, selector cũ được giữ hợp lệ trong đoạn chuyển; double fault dùng IST stack trong `.bss`.
- COM1 115200 8N1; panic in vector/error/RIP/CS/RFLAGS/RSP/GPR và CR2 khi #PF. PIC remap, PIT 100 Hz, IRQ0 nhận ba tick/EOI rồi mask và phát `M08:PASS`; chưa có timer API.
- `tests/m08/build_kernel.py` tạo ELF64 ET_EXEC higher-half, `p_paddr=0`, không undefined/dynamic runtime, kèm compiler/linker/commands/source+ELF hashes và inspection. `scripts/m07_image.py` nhận `--kernel-build`; đường mặc định M05 giữ nguyên.
- QEMU tests boot cùng artifact kernel/image qua SeaBIOS và OVMF; fault profile `ud` và `pf` yêu cầu đúng panic vector. Reports nằm dưới `out/m08/`; image/firmware nguồn được hash trước/sau run.

## Kết quả local hiện có

- Kernel build/provenance và host checks: **PASS**, 3/3. M08 guest: **PASS**, 6/6 thật trên BIOS/UEFI (normal, `#UD`, guard `#PF`); panic serial có vector mong đợi, PF có `CR2=guard`.
- Hồi quy local: M05 Debug/Release, M06 **3/3**, M07 **2/2**, M03 Debug/Release **4/4** và ASan/UBSan **3/3**, M04 **2/2** đều **PASS**; M02 **2 PASS, 2 SKIPPED**. Host pytest sau sửa review: **266 passed, 137 deselected**; các guest được chạy qua CTest M08.
- Docs validator **PASS**, unittest tài liệu **36/36 PASS**. M01 bootstrap local **NOT RUN**; Toolchain CI đã chạy bootstrap và toàn regression.
- Bốn workflow SUCCESS trên SHA nghiệm thu `56cbc3bd290d6b76789a50a8c80762896b57f4d4`: [Documentation CI](https://github.com/duclam2011dz/LamixOS/actions/runs/40882457559), [Harness CI](https://github.com/duclam2011dz/LamixOS/actions/runs/40882457555), [Toolchain CI](https://github.com/duclam2011dz/LamixOS/actions/runs/40882457670), [Boot Contract CI](https://github.com/duclam2011dz/LamixOS/actions/runs/40882457776).
- Review đã phát hiện thiếu ACPI span trước `lbp_validate()`; sửa ở `56cbc3b`, phản hồi và resolve thread. OVMF local bỏ RSDP không căn 16 byte nên guest path ACPI hợp lệ **NOT RUN**; M03 span tests và host structural test đã chạy. PC thật và PIC/PIT trên phần cứng **NOT RUN**.

## Lệnh chính

```bash
python tests/m08/build_kernel.py --build build/m08
PATH="$PWD/.venv/bin:$PATH" .venv/bin/cmake -S tests/m08 -B build/m08-tests -G Ninja -DPython3_EXECUTABLE="$PWD/.venv/bin/python"
PATH="$PWD/.venv/bin:$PATH" .venv/bin/ctest --test-dir build/m08-tests --no-tests=error --output-on-failure
```

Không đổi BIOS Stage1 gate, Stage2 budget, LBP wire format hoặc kernel entry ABI trong M08. (Tại thời điểm lưu phần lịch sử này, M09 chưa bắt đầu.)

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

Bước tiếp theo sau M08 là M09; triển khai M09 và bằng chứng hiện hành ở đầu file.

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

Nếu cần thay LBP wire layout, kernel ABI, BIOS gate/budget, policy A/B hoặc scope milestone đã duyệt thì dừng và xin duyệt kế hoạch bổ sung. Rollback bằng commit revert; không reset/clean, force-push hoặc di chuyển tag đã push. M08–M11 DONE; M12 VERIFYING.
