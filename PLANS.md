# Kế hoạch và kết quả — M24 (M24 DONE; M23/M22/M21/M20 DONE; M18 PLANNED)

Trạng thái milestone: M00=DONE; M01=DONE; M02=DONE; M03=DONE; M04=DONE; M05=DONE; M06=DONE; M07=DONE; M08=DONE; M09=DONE; M10=DONE; M11=DONE; M12=DONE; M13=DONE; M14=DONE; M15=DONE
Trạng thái M16: DONE; M17: DONE; M18: PLANNED; M19: DONE; M20: DONE; M21: DONE; M22: DONE; M23: DONE; M24: DONE; M25–M44: PLANNED

## M24 — SMP: AP startup và per-CPU scheduler (DONE, 2026-11-01)

- Profile `m24-kernel` bổ sung MADT xAPIC discovery, AP INIT/SIPI trampoline, per-CPU GDT/TSS/IST/syscall/boot stacks, per-CPU scheduler run queues, affinity/work stealing, SMP locks, AP timer-start acknowledgement và IPI TLB shootdown. Giữ profile BSP-only, `LATEST_PROFILE=m20`, M18, LBP v1 và syscall ABI; không dùng x2APIC.
- Cross-build **PASS**. M24 CTest local **4/4 PASS** (native ASan/UBSan, host provenance, guest matrix/fault injection, scheduler TSan); guest SeaBIOS `pc` và OVMF `q35` × 1/2/4/8 vCPU × 3 lượt **PASS**. Fault cases AP timeout, topology vượt giới hạn và TLB acknowledgement timeout **PASS**. Regression M17 **3/3**, M19 **3/3**, M20 **3/3**, M21 **3/3**, M22 **4/4**, M23 **3/3 PASS**. Docs validator **PASS**, unittest tài liệu **41/41 PASS** trước closure-doc PR.
- Review phát hiện bốn P1 về outgoing stack lifetime, GS state trong interrupt/syscall context switch, TLB shootdown chờ lock và timer race quanh `lock_waiting`; các sửa đổi đã được kiểm tra trên M24 CTest và các review threads resolve sau khi người dùng cho phép. Chưa có receipt review mới trên commit cuối `ecbeef...`.
- GitHub Actions PR #28: Toolchain [41313786645](https://github.com/duclam2011dz/LamixOS/actions/runs/41313786645) **SUCCESS** ở attempt 2; attempt 1 **FAIL** do thiếu marker `M23:APP_ALPHA:PASS` trong một test UEFI/8 vCPU, dù log cùng lượt có `M23:PASS` sau cleanup. Attempt 2 chạy lại toàn bộ Toolchain job, gồm M24 CTest, và **SUCCESS**. Harness [41313786650](https://github.com/duclam2011dz/LamixOS/actions/runs/41313786650), Documentation [41313786705](https://github.com/duclam2011dz/LamixOS/actions/runs/41313786705), Boot Contract [41313786583](https://github.com/duclam2011dz/LamixOS/actions/runs/41313786583) đều **SUCCESS**.
- PR [#28](https://github.com/duclam2011dz/LamixOS/pull/28) merge commit `590ebae8eacaa1f0a582051e67bae750a717bdcc`; acceptance commit `ecbeef72846e473e1b8e0cb572a086bd7e7c5dc9`; annotated tag `milestone/M24` object `7dd2b905a08e93320146cd041ed28c033b813b28` trỏ acceptance commit. Bằng chứng/giới hạn tổng hợp tại [handoff](docs/SESSION_HANDOFF.md). Concurrent teardown/address-space-switch race, audit độc lập và PC thật/firmware ngoài profile QEMU **NOT RUN**. M25 còn PLANNED; chưa lập kế hoạch hoặc triển khai.

## M23 — Static ELF64 loader/user runtime (DONE, 2026-11-01)

- Thêm profile `m23-kernel` tách biệt, giữ `LATEST_PROFILE=m20` và profile M22 nguyên trạng. Kernel artifact nhúng read-only `newc CPIO` gồm hai app. Loader kiểm tra ELF64 ET_DYN x86-64, giới hạn segment/bounds/alignment/entry/W^X, từ chối interpreter và metadata/relocations ngoài subset; áp dụng `R_X86_64_RELATIVE` với load bias xác định, không ASLR. Process map nhiều segment, BSS zero, stack `argc/argv/envp` + auxv, và thu hồi các trang do loader sở hữu.
- Thêm `crt0`, libc freestanding tối thiểu, hai app kiểm tra argv/envp/BSS/output/exit, fixture malformed ELF, CPIO reader, profile/build/provenance, QEMU profile và Toolchain CI step. Không đổi LBP v1, ABI 1.1, M22 hoặc `latest`; không bắt đầu M24.
- Cross-build `m23-kernel` **PASS**; kernel ELF SHA-256 `ea24453d41e061a691e32e95592d45c2dffef516cb482df53d41e5b25c3580ae`; initramfs CPIO `2545bc1ba2b0abb4bff2de196963d59042b40db213ba2c3e02463c16833e7b15`; app ELF hash/provenance ở `build/m23/build.json`. CTest **3/3 PASS**: ASan/UBSan native, host provenance và guest BIOS/UEFI. Guest SeaBIOS `pc` 3/3 và OVMF `q35` 3/3 **PASS**; log có `M23:MALFORMED_ELF:PASS`, hai app markers, process isolation/cleanup và `M23:PASS`. Beta ELF giữ `.bss` NOBITS và guest đọc volatile BSS buffer 4 KiB+.
- Review cuối phát hiện relocation hợp lệ trỏ one-past-end bị từ chối và compiler tối ưu bỏ mất fixture BSS cũ. Loader nay cho phép giá trị đúng `image_end` trong khi vẫn chặn overflow/ngoài user range; native regression kiểm tra relocation đó. App beta đọc volatile BSS buffer vượt một trang; readelf xác nhận `.bss` NOBITS và guest BIOS/UEFI đều PASS.
- Native direct sanitizer build **PASS**; Python syntax checks và freestanding `process.c` GCC compile **PASS**. Regression CTest M17 **3/3**, M19 **3/3**, M20 **3/3**, M21 **3/3**, M22 **4/4** đều **PASS**; docs validator **PASS**, unittest tài liệu **41/41 PASS**. Review trên PR head `2861267e38878e9c2cd3275b11cf3d7e177af2f5` phát hiện hai lỗi P2: relocation one-past-end và fixture BSS bị tối ưu bỏ; cả hai đã sửa trong commit `3c46863`, regression native/guest M23 CTest **3/3 PASS**. Trên acceptance SHA `ee3015379ecc9d3bc350b922c5776844d8ba80fe`, Toolchain [41287980720](https://github.com/duclam2011dz/LamixOS/actions/runs/41287980720), Harness [41287980714](https://github.com/duclam2011dz/LamixOS/actions/runs/41287980714), Documentation [41287980716](https://github.com/duclam2011dz/LamixOS/actions/runs/41287980716), Boot Contract [41287980712](https://github.com/duclam2011dz/LamixOS/actions/runs/41287980712) đều **SUCCESS**; Codex review cuối sạch, mọi thread đã resolve. PR [#26](https://github.com/duclam2011dz/LamixOS/pull/26), acceptance SHA, merge commit và annotated tag đã được xác minh; provenance đầy đủ ở [handoff](docs/SESSION_HANDOFF.md). `cmake` mặc định thiếu trên PATH, dùng `.venv/bin/cmake`; Ninja không có nên CTest local dùng generator `Unix Makefiles`. PC thật, firmware ngoài profile QEMU và audit bảo mật độc lập **NOT RUN**.
- M23 acceptance commit `ee3015379ecc9d3bc350b922c5776844d8ba80fe`; PR #26 merge commit `8c844cb85cca203051d2aa88a3629a992ccb96eb`; tag object và CI tại [handoff](docs/SESSION_HANDOFF.md). PC thật, firmware ngoài QEMU và audit bảo mật độc lập **NOT RUN**. M18 vẫn PLANNED; chưa bắt đầu M24.

## M22 — Handle-based capabilities và IPC (DONE, 2026-11-01)

- Profile riêng `m22-kernel`: process-local opaque 64-bit generation handles, rights subset, table tối đa 256 slots; full-duplex bounded channels, nonblocking FIFO, attachment MOVE/DUP và anonymous SHM 1–16 page. ABI 1.1 append syscall IDs 4–11, wrapper C freestanding; M21 giữ ABI 1.0, `latest` vẫn M20. Không có process spawn, ELF loader/runtime, SMP, capability graph services hay đổi LBP v1/entry ABI.
- Code review tìm hai lỗi: channel endpoint attachment có thể tạo self/cross-core reference cycle làm rò heap; hai sender cạnh tranh MOVE có thể cùng thành công. Sửa bằng queue-edge cycle detection trên graph lock và `lamix_handle_transfer_batch()` atomic validate/transfer cùng enqueue. Native regression chứng minh self-cycle/cycle A→B→A bị từ chối, allocator count quay về baseline sau teardown, hai MOVE cạnh tranh chỉ có một success; review threads đã resolve.
- M22 freestanding cross-build **PASS**. Native ASan/UBSan và TSAN, host provenance, guest BIOS/UEFI trong M22 CTest **4/4 PASS**; guest SeaBIOS `pc` và OVMF `q35` **3/3 mỗi firmware PASS**. Guest kiểm two-process capability transfer, channel send/receive, SHM shared data, read-only fault isolation và process cleanup. Hồi quy guest BIOS/UEFI M17/M19/M20/M21 **1/1 mỗi profile PASS**; Toolchain CI chạy full regression. Docs validator **PASS**, unittest tài liệu **41/41 PASS** trước closure. CTest đặt `ASAN_OPTIONS=detect_leaks=0` trong môi trường ptrace; leak cycle regression dùng allocator accounting riêng.
- Acceptance SHA `0d2c4b62e860ba336da439abf03c8cd6b421157c`; PR [#24](https://github.com/duclam2011dz/LamixOS/pull/24) merge commit `d2c3be428f69d439caad901e82c738102fede05e`. Annotated tag `milestone/M22` object `580ffdf3824102ed2bdb89ddf7e825364a8a00b7`, peeled acceptance SHA. Bốn workflow **SUCCESS** trên acceptance SHA: Toolchain [41277685047](https://github.com/duclam2011dz/LamixOS/actions/runs/41277685047) (attempt 2), Harness [41277685300](https://github.com/duclam2011dz/LamixOS/actions/runs/41277685300), Documentation [41277685046](https://github.com/duclam2011dz/LamixOS/actions/runs/41277685046), Boot Contract [41277685160](https://github.com/duclam2011dz/LamixOS/actions/runs/41277685160). Toolchain attempt 1 thất bại ở regression M08 UEFI page-fault vì serial thiếu `cr2`; lần retry job đạt toàn bộ.
- Giới hạn còn lại: guest không thử mọi tổ hợp syscall invalid pointer/rights/overflow; allocation failure ở từng điểm teardown chưa được fault-inject exhaustive; audit bảo mật độc lập và PC thật/firmware ngoài profile QEMU **NOT RUN**. Đây không phải public SDK cấp cao. M18 vẫn PLANNED; M23 đã nghiệm thu sau đó. Rollback nếu cần là revert PR M22; không sửa/move tag M21/M22.

## M21 — Lamix-specific syscall ABI v1 (DONE, 2026-11-01)

- Profile riêng `m21-kernel`; `scripts/lamix_qemu.py --profile m21` build image dưới `out/m21/`, trong khi `latest` vẫn M20. Thêm `kernel/syscall.c/.h`, `kernel/syscall_entry.asm` và freestanding wrapper `user/include/lamix/syscall.h`, `user/syscall.c`. IDs triển khai: version=0, yield=1, exit=2, debug_write=3; version 1.0, lỗi signed negative, giới hạn write 256 byte. Không có syscall nhận handle; không đổi LBP v1/entry ABI, không bắt đầu M22/M23/M24/M18.
- Entry kiểm tra CPUID syscall, cấu hình EFER/STAR/LSTAR/FMASK theo GDT selector hiện có; dùng kernel stack thread và IST riêng cho NMI/#DB/#MC. RFLAGS được lọc; RIP/RSP user được query qua address space trước return; dùng `SYSRETQ` cho đường thường, `IRETQ` cho fallback. Copy debug buffer xác minh quyền U/S + read theo trang, kiểm tra overflow/unmapped mà không dereference user pointer trực tiếp. Kernel lỗi user return kết thúc process.
- M21 native ASan/UBSan **PASS**; provenance/host pytest **1/1 PASS**; guest BIOS/UEFI **3/3 mỗi firmware PASS**. Guest kiểm version, wrapper, `debug_write`, null/unmapped/cross-page/overflow/oversize, syscall lạ, yield, DF masking, exit, fallback IRETQ, user return RSP lỗi, process isolation và kill/wait. M20 regression guest **6/6 PASS**, M19 guest **6/6 PASS**, M17 guest **4/4 PASS**; M19/M20 native sanitizer **PASS**. Profile build M21 **PASS**.
- PR [#22](https://github.com/duclam2011dz/LamixOS/pull/22) merge commit `20073de921c8a13572c57ca9f106ed83cdd4f886`; acceptance commit `6793d8f0a622fcd9f4100921e7932fcb46414540`; annotated tag `milestone/M21` object `f52683213c48c8851f31e8aef653462d204d38ad` trỏ acceptance commit.
- Bốn workflow CI SUCCESS trên acceptance SHA: Toolchain [41270742769](https://github.com/duclam2011dz/LamixOS/actions/runs/41270742769), Harness [41270742714](https://github.com/duclam2011dz/LamixOS/actions/runs/41270742714), Documentation [41270742671](https://github.com/duclam2011dz/LamixOS/actions/runs/41270742671), Boot Contract [41270742711](https://github.com/duclam2011dz/LamixOS/actions/runs/41270742711). M21 CTest **3/3 PASS**; guest BIOS/UEFI **3/3 mỗi firmware PASS**; regression guest M17 4/4, M19 6/6, M20 6/6 PASS; local docs validator và unittest 41/41 PASS. PC thật/firmware ngoài profile QEMU **NOT RUN**.
- Profile M21 riêng, `latest` vẫn trỏ M20; rollback bằng revert các commit M21, không di chuyển tag đã push. Image/report M21 ở `out/m21/`. Xem [API](docs/API.md), [TESTING](docs/TESTING.md#lamix-syscall-abi-v1-m21), [handoff](docs/SESSION_HANDOFF.md).
- Rollback: đảo ngược riêng các thay đổi M21 bằng revert sau khi đã commit; giữ `LATEST_PROFILE=m20`, không di chuyển tag `milestone/M20`. Build/image/report M21 chỉ nằm dưới `build/` và `out/`.

## M20 — Processes, ring 3 và cô lập address space (DONE, 2026-11-01)

- Thêm `kernel/process.c`/`.h`: process sở hữu VMM root, code/data/stack frame và một scheduler thread; API nội bộ tạo, ghi nhận user fault, yêu cầu kill, đọc trạng thái wait và thu hồi sau khi thread rời context. Mỗi process dùng user VA ở PML4 slot riêng; supervisor mappings của kernel được chia sẻ nhưng giữ quyền supervisor. Không đổi LBP v1, entry ABI, syscall ABI, ELF loader hay M18.
- Mở rộng VMM với `VMM_USER`, kiểm tra user range thấp/canonical và map/protect U/S ở leaf lẫn intermediate entries. Process root chia sẻ các nhánh supervisor của kernel, giữ slot user riêng; hủy root chỉ thu hồi page tables do process sở hữu.
- Mở rộng scheduler với user interrupt frame, context-switch callback, `TSS.rsp0`/CR3 cập nhật theo thread và terminate-current từ interrupt. Profile M20 nhúng hai payload ring 3: cùng virtual data address nhưng physical backing khác; một payload ghi vào vùng chỉ process kia map và bị fault, process còn lại tiếp tục rồi bị kernel test manager kill.
- `m20-kernel` cross-build **PASS**; `scripts/lamix_qemu.py --profile m20 --build-only` **PASS**, tạo image BIOS/UEFI dùng chung trong `out/m20/`. Native process/VMM ASan+UBSan **PASS**; M20 host pytest **1/1 PASS**. Guest QEMU cùng image/kernel BIOS/UEFI **3/3 mỗi firmware PASS**, serial gồm `M20:USER_FAULT:EXPECTED`, `M20:ISOLATION:PASS`, `M20:KILL_WAIT:PASS`, `M20:PASS`; image/firmware/build inputs không đổi và cleanup sạch. M19 pytest **7/7 PASS**; M19 native sanitizer PASS; M17 guest regression **4/4 PASS**.
- PR [#20](https://github.com/duclam2011dz/LamixOS/pull/20) merge commit `88821fdd59a6341b636e37e7e9a045f536d218a8`; acceptance commit `d0b97be7566e793bc0c54810ec574903ae2ed3dc`; annotated tag `milestone/M20` object `651e6ea9b065ac9a3759399fe9e4996511a0ce55` trỏ acceptance commit. Bốn workflow SUCCESS trên acceptance SHA: Toolchain [41265639668](https://github.com/duclam2011dz/LamixOS/actions/runs/41265639668), Harness [41265639777](https://github.com/duclam2011dz/LamixOS/actions/runs/41265639777), Documentation [41265639726](https://github.com/duclam2011dz/LamixOS/actions/runs/41265639726), Boot Contract [41265639667](https://github.com/duclam2011dz/LamixOS/actions/runs/41265639667). Toolchain CI CTest/build and BIOS/UEFI guest profile M20 **PASS**.
- Local docs validator **PASS**, unittest **41/41 PASS**; native ASan/UBSan và guest BIOS/UEFI **3/3 mỗi firmware PASS**. Local CMake/CTest wrapper **NOT RUN** do thiếu `cmake`; CTest đã chạy thành công trong Toolchain CI. PC thật và firmware ngoài profile QEMU **NOT RUN**. M20 `DONE`; M18 vẫn `PLANNED`; M21 đã nghiệm thu.

## M19 — Preemptive priority round-robin scheduler BSP (DONE)

- Thêm `kernel/scheduler.c`/`.h`: TCB, bootstrap/idle, stack 16 KiB từ kernel heap, bốn ready queue priority, RR theo tick, aging mỗi 100 tick, mutex non-recursive, FIFO theo effective priority, direct handoff, priority inheritance/deadlock detection và trì hoãn thu hồi stack thread đã exit.
- Profile `m19-kernel` mở rộng M17, không bật/đòi hỏi M18. LAPIC timer vector `0x31` gọi scheduler sau đúng một EOI; software reschedule vector `0x34` không gửi EOI. NASM nhận frame được chọn; synthetic frame gồm RSP/SS theo long-mode IRETQ. `-mgeneral-regs-only` và boot contracts giữ nguyên.
- Có native scheduler tests với ASan/UBSan và guest harness BIOS/UEFI ba lần mỗi profile. Cross-build **PASS**; M19 CTest trên CI **3/3 PASS**; local native+host CTest **2/2 PASS**; guest BIOS/UEFI **3/3 mỗi firmware PASS** với CPU-bound workers, priority aging 300 tick và mutex inheritance/direct handoff/wakeup. M17 guest regression **4/4 PASS**. Native tests bao gồm waiter FIFO, giữ FIFO khi effective priority không đổi, transitive inheritance, self-lock, non-owner unlock, exit khi đang giữ mutex và OOM.
- Khi chạy guest đầu tiên, frame ban đầu thiếu `RSP`/`SS`, gây `#GP` tại `iretq`. Đã sửa frame 160→176 byte, thêm stack pointer canonic và `SS=0x10`; guest BIOS thử lại đã đạt marker M19. Đây là test finding đã sửa, không tính lần lỗi là PASS.
- Codex review trên acceptance SHA **clean**, mọi finding đã sửa và review thread resolve. Docs validator và unittest **41/41 PASS**. Bốn workflow CI SUCCESS trên acceptance SHA `9a0861ec4b152e189144251ece884bc6ffd466d5`: Toolchain [41213184919](https://github.com/duclam2011dz/LamixOS/actions/runs/41213184919), Harness [41213184786](https://github.com/duclam2011dz/LamixOS/actions/runs/41213184786), Documentation [41213184903](https://github.com/duclam2011dz/LamixOS/actions/runs/41213184903), Boot Contract [41213184777](https://github.com/duclam2011dz/LamixOS/actions/runs/41213184777). PR [#18](https://github.com/duclam2011dz/LamixOS/pull/18) merge commit `c550210f4ba69f7181ed4a3cf0001e20059ea52a`; annotated tag `milestone/M19` object `6d055839a1dce6356eba49c0e2a22e6630708b8a` trỏ acceptance SHA. PC thật và firmware ngoài profile QEMU **NOT RUN**.

## M17 — VirtIO transport + VirtIO-GPU 2D (DONE, 2026-10-31)

- Thêm `kernel/virtio_gpu.c`/`.h`: split virtqueue, modern PCI common/notify capabilities, VERSION_1 negotiation và GPU 2D resource/create, backing, scanout, transfer/flush. Kernel M17 dò VirtIO-GPU PCI, map BAR uncached, cấp backing từ kernel heap/PMM/VMM lifecycle và ưu tiên GPU; framebuffer M16/serial-only là fallback. LBP v1, entry ABI và profile cũ giữ nguyên.
- Thêm profile `m17-kernel`, host sanitizer tests trên cùng implementation, host provenance/image checks, guest tests và QMP screendump PPM→PNG. Toolchain CI có job M17 và lưu reports/screenshots; không upload disk image hay firmware.
- Sau khi chuyển request/response DMA sang vùng persistent và thêm late-response sanitizer regression, kernel ELF SHA-256 hiện tại `5d6e8d50ed798d6c4bd6de857f2132f1e3d521b5ad2c04a065d7672c5d9f1e3d`. Image raw GPT dùng chung BIOS/UEFI: `out/m17/lamix-m17-5d6e8d50ed79.img`; SHA-256 chưa tính toàn image sparse 8 GiB, harness bảo vệ các extents cần thiết trong mỗi guest run. Chạy QEMU bằng lệnh ở [TESTING](docs/TESTING.md#virtio-gpu-2d-m17).
- M17 CTest **3/3 PASS**: `m17-native-sanitize`, `m17-host`, `m17-bios-uefi-guest`; guest pytest **4/4 PASS**: GPU BIOS/UEFI và fallback BIOS/UEFI. Ảnh guest được chụp sau present và kiểm tra trực quan: BIOS `out/m17/runs/run-9_1xpo9k/screen.png`, UEFI `out/m17/runs/run-ssviqhl7/screen.png`. Cả hai có nền tối, panel xanh, khung trắng, chữ `LAMIX OS M17` và đường cam. PPM SHA-256 cùng nhau `384f38fe8378e781c72664243afa6981a248fa73c2defdcb1d8d42a49cc5e012`.
- Hồi quy M15 host/sanitizer **2/2 PASS**, M16 host/sanitizer **2/2 PASS**, QMP harness fixtures **1/1 PASS**, M07 host **7/7 PASS** (một QEMU case deselected); BIOS Stage2 và UEFI regression build-only PASS. Lần chạy M07 ban đầu dùng build provenance cũ nên FAIL; suite được chạy lại trực tiếp với build mới trong `build/m17-regression-*` và đạt.
- Docs validator **PASS**, unittest tài liệu **41/41 PASS**. Automated review phát hiện hai finding P1: liên kết artifact local khiến docs validator fail, và response DMA buffer có vòng đời stack sau timeout. Cả hai đã sửa; request/response dùng persistent storage, sanitizer có regression response đến muộn sau reset. Hai review thread đã resolve. Bốn workflow CI SUCCESS trên acceptance SHA `292eb59f3c368c2933a7dbb0d135b89f4fb74415`: Toolchain [41200982371](https://github.com/duclam2011dz/LamixOS/actions/runs/41200982371), Harness [41200982402](https://github.com/duclam2011dz/LamixOS/actions/runs/41200982402), Documentation [41200982454](https://github.com/duclam2011dz/LamixOS/actions/runs/41200982454), Boot Contract [41200982320](https://github.com/duclam2011dz/LamixOS/actions/runs/41200982320). PR [#15](https://github.com/duclam2011dz/LamixOS/pull/15) merge commit `8afa222fc493c2adbac71bb415adacc03b4510f7`; annotated tag `milestone/M17` trỏ acceptance SHA (tag object được xác minh sau khi push). PC thật và firmware ngoài profile QEMU **NOT RUN**.
- Follow-up sau phản hồi màn hình đen: serial dừng tại `M15:READY` vì profile M17 chờ synthetic PS/2 input trước khi khởi tạo đồ họa. M17 nay bỏ qua self-test tương tác M15 (`M15:SELFTEST:SKIPPED`), còn profile M15 không đổi; guest M17 kiểm tra boot trực tiếp không cần QMP input. CTest 3/3, guest GPU/fallback BIOS/UEFI 4/4, M15 guest regression 2/2, docs validator và unittest 41/41 PASS. Người dùng xác nhận boot hiển thị bình thường qua BIOS và UEFI. Bốn workflow SUCCESS trên SHA `9c7b7f484be79c9b22c922ac1ff0d2b316b88e3e`: Toolchain [41206133212](https://github.com/duclam2011dz/LamixOS/actions/runs/41206133212), Harness [41206133271](https://github.com/duclam2011dz/LamixOS/actions/runs/41206133271), Documentation [41206133306](https://github.com/duclam2011dz/LamixOS/actions/runs/41206133306), Boot Contract [41206133141](https://github.com/duclam2011dz/LamixOS/actions/runs/41206133141). PR [#16](https://github.com/duclam2011dz/LamixOS/pull/16) merge commit `d290ef5ab6547b6b0228e38ed5571054dea16ecb`; tag `milestone/M17` giữ nguyên acceptance commit, không tạo lại hay di chuyển.
- Thêm [scripts/lamix_qemu.py](scripts/lamix_qemu.py), mặc định profile mới nhất M19, hỗ trợ BIOS/UEFI và vẫn chọn được M17. `--build-only` **PASS**, tạo image raw GPT chung firmware ở `out/m19/`; guest M19 boot được kiểm chứng riêng bằng harness. Lệnh chạy và yêu cầu firmware/toolchain ghi tại [TESTING](docs/TESTING.md#scheduler-bsp-m19).

## M16 — Software framebuffer renderer (DONE)

- Thêm renderer freestanding `kernel/graphics.c` với backbuffer XRGB8888, channel-mask conversion, clipping, pixel/line/fill/outline, ASCII 8×16 bitmap font, union damage rectangle, present và FNV-1a 64-bit checksum. Backbuffer cấp từ kernel heap; framebuffer thiết bị không đi qua PMM.
- BIOS M16 profile gọi VBE, ưu tiên 1024×768×32 rồi 800×600×32, phát LBP framebuffer record v1 và map vùng uncached; VBE không bật trong profile cũ. UEFI tiếp tục GOP hiện hành. Stage1 và wire ABI không đổi.
- Kiểm thử native ASan/UBSan xác nhận checksum golden, ASCII printable, mask conversion, pitch, clipping, damage và bounds lỗi. M16 CTest local hiện 3/3 PASS; guest cùng kernel/image SeaBIOS `pc` và OVMF `q35` mỗi profile 1/1 PASS, có marker render/present/checksum và protected hashes/cleanup sạch.
- BIOS Stage2 Release build **PASS**: core file 42,024 byte, memory 176,128/262,144 byte; bootstrap 32,768 byte. Kernel cross-build/provenance **PASS**. Regression M07 + Stage2 host + M15 host: **54 PASS, 3 QEMU cases deselected**. Docs validator **PASS**, unittest tài liệu **41/41 PASS**.
- PR #14 merge commit `e37b0b6c3f5423ae518c32e2fc325a2dd5a4a19b`; acceptance commit `09c4248dece6273295c28639090883d00a3af9c0`; annotated tag `milestone/M16` object `a66b7f9a0fbf1d52cb3df1be2b9d09e0c0862a63` trỏ acceptance commit. Bốn workflow SUCCESS trên SHA nghiệm thu: Toolchain [41197034264](https://github.com/duclam2011dz/LamixOS/actions/runs/41197034264), Harness [41197034242](https://github.com/duclam2011dz/LamixOS/actions/runs/41197034242), Documentation [41197034318](https://github.com/duclam2011dz/LamixOS/actions/runs/41197034318), Boot Contract [41197034328](https://github.com/duclam2011dz/LamixOS/actions/runs/41197034328). Review sạch sau khi sửa VBE 3.0 linear pitch/masks. PC thật/firmware ngoài profile QEMU **NOT RUN**.

## M15 — Input bàn phím và chuột PS/2 (DONE)

### Triển khai hiện có

- Thêm `kernel/input.c`/`kernel/input.h`: Set 1 đã dịch, keycode chuẩn hóa, make/break, modifiers, Caps Lock, ASCII US QWERTY, PS/2 mouse packet 3 byte và SPSC queue 128 event với sequence/overflow stats. Thêm `kernel/ps2.c`/`kernel/ps2.h` với polling giới hạn, keyboard bắt buộc và mouse tùy chọn.
- Profile `m15-kernel` tích hợp IRQ1/IRQ12 vào xAPIC/IOAPIC M14, vector riêng và một LAPIC EOI mỗi IRQ. Các profile cũ giữ đường cũ. Không thêm syscall, LBP hay entry ABI.
- `scripts/qemu_harness.py` hỗ trợ QMP script sau marker; `tests/m15/` có native sanitizer, provenance và cùng kernel/image guest BIOS/UEFI. Toolchain CI đã thêm job và upload report.

### Kết quả xác minh local

- Kernel cross-build `m15-kernel` **PASS**, ELF SHA-256 `8fc8d6af1f7847036510b2925ea5bde4de7718487e3cfc4cd7bfd894a149bc60`; ELF có 3 load segments, không undefined symbols. BIOS Stage2 core 172,032/262,144 bytes, bootstrap giữ giới hạn 32 KiB; UEFI build-only **PASS** trong fixture guest.
- M15 CTest host sanitizer + host/provenance **2/2 PASS**. Guest SeaBIOS `pc` và OVMF `q35` cùng kernel/image **3/3 PASS mỗi profile**, có M15 READY/keyboard/mouse/queue/PASS; protected hashes không đổi, cleanup sạch. Reports: `out/m15/results/bios.json`, `out/m15/results/uefi.json`.
- QMP harness unit tests **35/35 PASS**. Hồi quy M07 2/2, M08 2/2, M09 3/3, M10 3/3, M11 3/3, M12 6/6, M13 3/3, M14 3/3 **PASS**. Docs validator **PASS**.
- Bốn workflow GitHub Actions SUCCESS trên acceptance SHA `831748b5d65f0a88394745b20da82a56f69439c4`: Toolchain [41180666554](https://github.com/duclam2011dz/LamixOS/actions/runs/41180666554), Harness [41180666561](https://github.com/duclam2011dz/LamixOS/actions/runs/41180666561), Documentation [41180666589](https://github.com/duclam2011dz/LamixOS/actions/runs/41180666589), Boot Contract [41180666560](https://github.com/duclam2011dz/LamixOS/actions/runs/41180666560). Codex Review finding delta chuột 9-bit đã sửa, test ±200 thêm và thread resolved. PR #13 merge commit `eba0607607545638fe1569fa129abac1b283d7d1`; annotated tag `milestone/M15` object `69aa3ebdb5c2843487b73925c8b73f96893cd2d7` trỏ acceptance SHA. PC thật/firmware ngoài profile QEMU **NOT RUN**. M15 DONE; M16 chưa bắt đầu.

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

Nếu cần thay LBP wire layout, kernel ABI, BIOS gate/budget, policy A/B hoặc scope milestone đã duyệt thì dừng và xin duyệt kế hoạch bổ sung. Rollback bằng commit revert; không reset/clean, force-push hoặc di chuyển tag đã push. M08–M17 DONE; M18–M44 PLANNED.
