# Kế hoạch và kết quả — M15

Trạng thái milestone: M00=DONE; M01=DONE; M02=DONE; M03=DONE; M04=DONE; M05=DONE; M06=DONE; M07=DONE; M08=DONE; M09=DONE; M10=DONE; M11=DONE; M12=DONE; M13=DONE; M14=DONE; M15=IN_PROGRESS

## M15 — Input bàn phím và chuột PS/2 (IN_PROGRESS)

### Triển khai hiện có

- Thêm `kernel/input.c`/`kernel/input.h`: Set 1 đã dịch, keycode chuẩn hóa, make/break, modifiers, Caps Lock, ASCII US QWERTY, PS/2 mouse packet 3 byte và SPSC queue 128 event với sequence/overflow stats. Thêm `kernel/ps2.c`/`kernel/ps2.h` với polling giới hạn, keyboard bắt buộc và mouse tùy chọn.
- Profile `m15-kernel` tích hợp IRQ1/IRQ12 vào xAPIC/IOAPIC M14, vector riêng và một LAPIC EOI mỗi IRQ. Các profile cũ giữ đường cũ. Không thêm syscall, LBP hay entry ABI.
- `scripts/qemu_harness.py` hỗ trợ QMP script sau marker; `tests/m15/` có native sanitizer, provenance và cùng kernel/image guest BIOS/UEFI. Toolchain CI đã thêm job và upload report.

### Kết quả xác minh local

- Kernel cross-build `m15-kernel` **PASS**, ELF SHA-256 `8fc8d6af1f7847036510b2925ea5bde4de7718487e3cfc4cd7bfd894a149bc60`; ELF có 3 load segments, không undefined symbols. BIOS Stage2 core 172,032/262,144 bytes, bootstrap giữ giới hạn 32 KiB; UEFI build-only **PASS** trong fixture guest.
- M15 CTest host sanitizer + host/provenance **2/2 PASS**. Guest SeaBIOS `pc` và OVMF `q35` cùng kernel/image **3/3 PASS mỗi profile**, có M15 READY/keyboard/mouse/queue/PASS; protected hashes không đổi, cleanup sạch. Reports: `out/m15/results/bios.json`, `out/m15/results/uefi.json`.
- QMP harness unit tests **35/35 PASS**. Hồi quy M07 2/2, M08 2/2, M09 3/3, M10 3/3, M11 3/3, M12 6/6, M13 3/3, M14 3/3 **PASS**. Docs validator **PASS**.
- GitHub Actions **PENDING/NOT RUN** trên thay đổi hiện tại; cần CI xanh trên acceptance SHA trước nghiệm thu. PC thật/firmware ngoài profile QEMU **NOT RUN**. M15 chưa DONE.

## M14 — Local APIC/IOAPIC và timer (DONE)

### Thực hiện

- Thêm RSDP validation chung cho loader BIOS/UEFI; BIOS quét EBDA/ROM trong giới hạn, UEFI thử ACPI 2.0 rồi ACPI 1.0 hợp lệ. Dùng record/feature LBP v1 hiện có, không đổi wire layout.
- Mở rộng MADT parser, thêm helper xAPIC cho ISA override, IOAPIC redirection và hiệu chuẩn LAPIC timer; profile kernel M14 yêu cầu ACPI. Mã xAPIC/IOAPIC tích hợp hiện có host-test và build evidence.
- Stage2 BIOS build-only **PASS**: core 172,032 byte/256 KiB; bootstrap 32 KiB. Chưa thay Stage1 hoặc budget.

### Kiểm chứng hiện có

- Kernel cross-build **PASS**; BIOS Stage2 build-only **PASS**; UEFI build-only **PASS**; host ASan/UBSan M14 **PASS**.
- Guest M14 cùng kernel/image: BIOS SeaBIOS `pc` 3/3 và UEFI OVMF `q35` 3/3 **PASS**, đầy đủ marker ACPI/APIC/IOAPIC/PIT reference/LAPIC timer/M14; protected hashes giữ nguyên, cleanup sạch.
- CTest M14 **3/3 PASS**; hồi quy M05 4/4, M07 2/2, M08 2/2, M09 3/3, M10 3/3, M11 3/3, M12 6/6, M13 3/3 **PASS**. Docs validator **PASS** và unittest **41/41 PASS**.
- Bốn workflow GitHub Actions **SUCCESS** trên acceptance SHA `f41d20df47b7e95ac9ac73027c7cd7a7395b67c5`: Toolchain [41177278867](https://github.com/duclam2011dz/LamixOS/actions/runs/41177278867), Harness [41177278863](https://github.com/duclam2011dz/LamixOS/actions/runs/41177278863), Documentation [41177278850](https://github.com/duclam2011dz/LamixOS/actions/runs/41177278850), Boot Contract [41177278957](https://github.com/duclam2011dz/LamixOS/actions/runs/41177278957). Toolchain CI xác nhận M14 CTest 3/3, regression M05 4/4 và M06 UEFI 3/3, cùng M07–M13 đều PASS.
- PR #12 merge commit `8360690506b1472d3b31959f36bb3a511e76282e`; annotated tag `milestone/M14` object `85876228135c5f1d971a1a30687e999ce5ddab39` trỏ acceptance commit `f41d20df47b7e95ac9ac73027c7cd7a7395b67c5`. Automated review trên commit cuối không có major findings; thread hiệu chuẩn LAPIC được sửa và resolve.
- M06 CTest local sau chỉnh test **NOT RUN** do workspace closure không có `cmake`/`ctest`; Toolchain CI chạy test M06 thành công trên acceptance SHA. PC thật/firmware ngoài profile QEMU **NOT RUN**. M15 bắt đầu sau khi M14 được đóng.

## M13 — ACPI, PCI và device framework (DONE, 2026-10-31)

### Thực hiện

- Thêm `kernel/acpi.c`/`kernel/acpi.h`: parser RSDP rev0/rev2, RSDT/XSDT, checksum/bounds, MADT variable entries và MCFG discovery. Kernel đọc bảng qua VMM uncached physical mapping; không reclaim ACPI pages.
- Thêm `kernel/pci.c`/`kernel/pci.h`: PCI mechanism #1, ECAM address calculation, MCFG segments, multifunction enumeration, BAR sizing với command decode/bus master tạm tắt và restore, uncached MMIO map API. Không cấp phát DMA buffer hay kích hoạt bus mastering.
- Thêm `kernel/device.c`/`kernel/device.h`: static registry PCI, binding placeholder và owner/generation token cho DMA ownership. M13 profile giữ bitmap làm allocator VMM/heap, chạy Buddy self-test M12; không đổi LBP/loader/ABI.
- Thêm `tests/m13/` host sanitizer, build provenance/image profile, BIOS/UEFI guest cùng kernel, M13 CTest và Toolchain CI report upload.

### Kết quả đã chạy

- Acceptance commit `12d3ef1ac9040f9d523bb05a82066dadcaec6ecf`; PR #11 merge commit `26f3b3698f803fbcc8c8a39e7c9b4f01a631a7e5`. Annotated tag `milestone/M13` object `ff2b1fbbae53d37af78aed566f381ac1b122f2d6` trỏ acceptance commit.
- M13 cross kernel build **PASS**; kernel ELF SHA-256 `95739a537da548a93a957a34b5567e95011a7c583d368bcccab22acae11bd8a3`.
- M13 local CTest **3/3 PASS**: ASan/UBSan native, host provenance/image, guest BIOS/UEFI. Hai guest boot thật cùng kernel, enumerate 5 PCI functions, register 5 devices, kiểm tra BAR mapping/query/unmap; protected hashes giữ nguyên, cleanup sạch.
- Bốn GitHub workflows **SUCCESS** trên acceptance SHA: [Toolchain CI](https://github.com/duclam2011dz/LamixOS/actions/runs/41170371900), [Harness CI](https://github.com/duclam2011dz/LamixOS/actions/runs/41170371899), [Documentation CI](https://github.com/duclam2011dz/LamixOS/actions/runs/41170371842), [Boot Contract CI](https://github.com/duclam2011dz/LamixOS/actions/runs/41170371849). Hồi quy local M07 2/2, M08 2/2, M09 3/3, M10 3/3, M11 3/3, M12 6/6 **PASS**; docs validator và unittest 40/40 **PASS**.
- Hai firmware guest ghi `M13:ACPI:ABSENT`; ACPI traversal guest **NOT RUN**. Host ASan/UBSan fixtures kiểm RSDP/RSDT/XSDT/MADT/MCFG, malformed tables, XSDT-to-RSDT fallback và ECAM nonzero start bus. Review hai lỗi ECAM offset/ACPI fallback đã sửa trong acceptance commit; review threads resolved.
- PC thật và firmware ngoài profile QEMU **NOT RUN**. Không đổi LBP v1, loader, entry ABI, PMM default, không reclaim LOADER/ACPI, không DMA buffer/bus mastering. Tại thời điểm nghiệm thu M13, M14 chưa bắt đầu.

### Giới hạn đã ghi nhận

Hai firmware guest hiện hành không cung cấp ACPI RSDP trong LBP (`M13:ACPI:ABSENT`), nên chỉ host tests chứng minh parser ACPI; đây là **NOT RUN** cho guest traversal, không phải PASS. LBP v1, loaders và handoff không đổi. Thay đổi cách cấp ACPI record sẽ cần kế hoạch/duyệt riêng.

## M12 — Kế hoạch và kết quả đã nghiệm thu

Trạng thái M12: **DONE (2026-10-30)**. PR #10 merge commit `385f835b5825528915080859156e963db15e6fd0`; acceptance commit/tag target `f3391d66eabc6b6d82a27800db6e229bb74f577b`; annotated tag object `b4a0ee9ed47e3e6b77a8658e5270d8af8338bffe`. Bốn workflow CI SUCCESS trên acceptance SHA: Toolchain [41084497139](https://github.com/duclam2011dz/LamixOS/actions/runs/41084497139), Harness [41084497177](https://github.com/duclam2011dz/LamixOS/actions/runs/41084497177), Documentation [41084497141](https://github.com/duclam2011dz/LamixOS/actions/runs/41084497141), Boot Contract [41084497179](https://github.com/duclam2011dz/LamixOS/actions/runs/41084497179). Bitmap M09 tiếp tục mặc định; Buddy là backend thử nghiệm riêng trong profile `m12-kernel`, VMM/heap chỉ dùng Buddy ở profile đó. Không đổi LBP v1, kernel entry ABI, loaders, BIOS Stage1 gate hoặc Stage2 budget.

## Phạm vi M12 — Buddy allocator thử nghiệm song song

- `kernel/pmm.c`/`kernel/pmm.h` thêm tree bitmap 2-bit/node cho frames dưới 4 GiB, boundary-aware coalescing và order 0–18; API bitmap M09 không đổi. Chỉ frame đầy đủ thuộc `LBP_USABLE` được đưa vào pool; reservations/hole/extent edges ngăn coalesce. Không reclaim LOADER/ACPI và không cấp RAM trên 4 GiB.
- Kernel profile `m12-kernel` dành 512 KiB cho node state và 64 KiB cho boundary capacity trong `.bss`; tổng reserve metadata **589,824 bytes**. Khởi tạo Buddy chỉ sau validation map/reservations. Profile M12 route VMM/heap callbacks qua wrapper order 0; các profile trước giữ bitmap.
- Tests trong `tests/m12/` dùng cùng `kernel/pmm.c` trên native ASan/UBSan, host provenance, deterministic benchmark với bitmap contiguous adapter, workload fragmentation/pressure/OOM và QEMU guest. Host benchmark không đại diện hiệu năng guest.
- Rollback bằng profile/backend bitmap; bitmap vẫn là backend mặc định. Không chuyển mặc định: mixed-order benchmark CI có throughput thấp hơn ở Buddy và fragmentation fixture hai backend cho kết quả giống nhau.

## Kết quả nghiệm thu M12

- Cross build `m12-kernel` **PASS** trong Toolchain CI; kernel ELF SHA-256 `8ed07725bfa1fb5bb8d226c9d85cfe2f7efec82d45efe614a85998cc2c9ef847`.
- M12 CTest **6/6 PASS** trong Toolchain CI: Buddy ASan/UBSan, host provenance/image, benchmark native và BIOS/OVMF guest. Cùng kernel boot hai firmware; serial phát M08–M12 PASS; hashes protected inputs giữ nguyên, cleanup không cưỡng bức. Toolchain regression M07 2/2, M08 2/2, M09 3/3, M10 3/3 và M11 3/3 **PASS**.
- Benchmark CI seed `0x12a110c`, trace hash `3ded994161041ff7`, 20,000 trace operations × 5 repetitions. Latency/throughput chỉ tính allocator calls, no-op báo riêng: 50,385 calls và 49,615 no-op mỗi backend, 50,385 latency samples. p50/p95/max bitmap 203/580/11,902 ns, Buddy 180/1,425/11,440 ns; throughput 4,469,397/2,136,508 allocator calls/s; mean init 1,124,849/10,944,121 ns. Metadata bitmap/Buddy 262,144/589,824 B. Đây là một host CI run, không phải guest performance claim. Fixture alternate-free: mỗi backend còn 2,048 frames, largest 4 KiB, order 1 allocation thất bại; không thấy fragmentation advantage trong workload này.
- Documentation validator, unittest tài liệu 40/40 và `git diff --check` **PASS** local sau khi cập nhật handoff. Bốn workflow CI SUCCESS trên acceptance SHA.
- PC thật/firmware ngoài profile QEMU **NOT RUN**. Không reclaim LOADER/ACPI, không cấp RAM trên 4 GiB, không chuyển bitmap mặc định.

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

Nếu cần thay LBP wire layout, kernel ABI, BIOS gate/budget, policy A/B hoặc scope milestone đã duyệt thì dừng và xin duyệt kế hoạch bổ sung. Rollback bằng commit revert; không reset/clean, force-push hoặc di chuyển tag đã push. M08–M14 DONE; M15 IN_PROGRESS; M16–M44 PLANNED.
