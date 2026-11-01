# ROADMAP — Lamix OS

> **Phiên bản kế hoạch:** 1.0 (bản thiết kế ban đầu, chưa triển khai)  
> **Cập nhật:** 2026-11-01
> **Ngôn ngữ tài liệu:** Tiếng Việt  
> **Trạng thái toàn dự án:** M00 DONE; M01 DONE; M02 DONE; M03 DONE; M04 DONE; M05 DONE; M06 DONE; M07 DONE; M08 DONE; M09 DONE; M10 DONE; M11 DONE; M12 DONE; M13 DONE; M14 DONE; M15 DONE; M16 DONE; M17 DONE; M18 PLANNED; M19 DONE; M20 DONE; M21 IN_PROGRESS; M22–M44 PLANNED; chưa có kernel sản phẩm hoàn chỉnh hoặc OS hoàn chỉnh
> **Nguồn thẩm quyền:** Các quyết định kiến trúc đã được chủ dự án chốt trong quá trình thiết kế. Khi mã nguồn và tài liệu chưa khớp, phải báo rõ sự khác biệt, không tự ý đổi kiến trúc.

Trạng thái milestone: M00=DONE; M01=DONE; M02=DONE; M03=DONE; M04=DONE; M05=DONE; M06=DONE; M07=DONE; M08=DONE; M09=DONE; M10=DONE; M11=DONE; M12=DONE; M13=DONE; M14=DONE; M15=DONE

## 0. Tầm nhìn và giới hạn

**Lamix OS** là hệ điều hành **Gaming native x86_64** phát triển dài hạn từ con số 0: bootloader BIOS/UEFI riêng, Hybrid Kernel, giao diện Gaming Glass lấy cảm hứng từ Aero Glass, SDK game 2D trước rồi mở rộng thành hệ sinh thái game phong phú, ổn định và có khả năng sử dụng thực tế. Các thành phần OS chính phải do dự án tự xây dựng; cho phép dùng toolchain, trình giả lập, firmware kiểm thử, debugger và công cụ tạo image trên host. Không lấy GRUB/Limine, kernel Linux hay OS/framework runtime hoàn chỉnh làm phần triển khai cốt lõi.

**Không thuộc phạm vi ban đầu:** chạy trực tiếp game Windows/Linux, tương thích Win32/Proton/Wine, 3D GPU thực trên nhiều dòng card, trình duyệt web hoàn chỉnh và thương mại hóa. Không tuyên bố "production-ready" hoặc "playable hoàn thiện" trước khi có kết quả kiểm thử.

**Các nguyên tắc xuyên suốt:**

1. **Plan-first bắt buộc:** Trước **mỗi lượt chat yêu cầu triển khai phase/milestone tiếp theo**, người dùng phải chủ động bật **Codex Plan Mode** (`/plan` nếu phiên bản hỗ trợ). Codex đọc ngữ cảnh liên quan, đưa kế hoạch cụ thể, nêu rủi ro, lệnh build/test và **chờ người dùng phê duyệt**; chỉ sau đó mới chuyển sang chế độ có quyền chỉnh sửa để triển khai. `AGENTS.md` không thể tự bật Plan Mode trong UI.
2. **Chỉ làm một milestone/sub-milestone được duyệt mỗi lần**, không tự chạy hết roadmap, không tự ý thêm tính năng ngoài phạm vi; nếu quá lớn thì chia nhỏ và duyệt lại.
3. **Chất lượng trước tốc độ:** build sạch, không panic ngoài dự kiến, kiểm chứng trong QEMU; khi thêm subsystem thì bổ sung kiểm thử thích hợp. Không tuyên bố pass khi chưa chạy.
4. **Hai đường boot là một sản phẩm:** BIOS và UEFI đều phải đưa về cùng phiên bản **Lamix Boot Protocol (LBP)** và cùng kernel entry contract. Chạy hồi quy cả hai khi sửa boot/kernel chung.
5. **Tự viết OS nhưng không tự viết toolchain:** NASM; GCC `x86_64-elf` cross-compiler (bổ sung `i686-elf` khi thật sự cần); GNU LD/Binutils; CMake + Ninja + Python; QEMU, GDB, CTest, pytest, clang-format, clang-tidy, cppcheck. Build/test chính trong **Windows + WSL2** trên filesystem Linux của WSL khi có thể.
6. **Tài liệu là bộ nhớ dài hạn:** Mỗi tài liệu một trách nhiệm; file chỉ dẫn phải ngắn, có liên kết và được đọc có chọn lọc, nhằm tiết kiệm usage của Codex.

## 1. Baseline kiến trúc không được tự ý thay đổi

| Lĩnh vực | Định hướng đã chốt |
|---|---|
| CPU, ngôn ngữ | x86_64; NASM cho ASM, C và freestanding C++ |
| Kernel | Hybrid: kernel core hiệu năng cao, dịch vụ user-space phù hợp |
| Boot | Legacy BIOS (16-bit → 32-bit → 64-bit) và UEFI x86_64 tự viết; LBP có phiên bản, `BootInfo` chuẩn hóa |
| Disk | **GPT + BIOS Boot Partition + EFI System Partition (FAT32) + System A + System B + User Data**; Protective MBR/backup GPT không được ghi đè |
| Scheduler | Preemptive priority-based round-robin, nâng cấp scheduling class/affinity sau |
| SMP | BSP-only trước; AP startup, per-CPU state, SMP và balancing sau |
| Interrupts | 8259 PIC bootstrap → Local APIC/IOAPIC → MSI/MSI-X |
| Memory | Bitmap Physical Memory Manager → Virtual Memory Manager → kernel heap → buddy allocator |
| Processes/IPC | Ring 3 processes, threads, handle-based IPC, address spaces độc lập |
| Syscalls | Lamix-specific x86_64 ABI, versioning, validation, user library wrapper |
| Executables | Static ELF64 trước; PIC/ASLR và dynamic linking sau |
| Security | Handle-based capabilities + process isolation; kiểm tra user pointers, NX/W^X và quyền truy cập từ sớm |
| Drivers | Hybrid driver architecture; **kernel-mode và user-mode drivers đều được giữ lâu dài**, không chuyển hết sang user-space |
| Graphics | Software rendering trước, VirtIO-GPU 2D trên QEMU **sớm**; acceleration/virgl giai đoạn tiếp theo sau khi có nền tảng |
| Desktop | User-space window manager/compositor; Gaming Glass/Aero Glass, dark theme, chế độ giảm hiệu ứng khi chơi game |
| Games | Chỉ native; Lamix 2D SDK + Pong đầu tiên; sau đó Snake, Breakout, platformer, các thể loại mới |
| Audio | Intel HDA + audio server user-space, để sau các milestone nền tảng |
| Filesystems | VFS + FAT32 + tmpfs trước; LamFS tối ưu hiệu năng sau, vẫn phải đảm bảo an toàn dữ liệu tối thiểu |
| Packages | `lpm` (Lamix Package Manager): offline trước, online sau networking |
| Networking | **Các phase muộn:** VirtIO-Net → Ethernet/ARP/IPv4/ICMP/UDP/TCP/DHCP/DNS/sockets; Intel E1000 và IPv6 sau |
| Updates | A/B system updates có health check và rollback, thực hiện khi storage/boot ổn định |
| Testing | pytest + CTest + QEMU + GDB + serial logs; ma trận BIOS/UEFI và cấu hình tương ứng; không giả kết quả |

### 1.1 Các ràng buộc kỹ thuật cần xác minh khi triển khai

- **UEFI:** Ứng dụng EFI theo PE/COFF và calling convention firmware thích hợp. Trước `ExitBootServices()` cần lấy memory map + map key hiện hành; sau khi exit thành công không gọi Boot Services, bảo toàn vùng firmware bắt buộc. Chuẩn hóa memory map trước khi chuyển cho kernel.
- **BIOS + GPT:** Chứa boot code trong vùng MBR phù hợp, stage bổ sung trong BIOS Boot Partition; không dùng “post-MBR gap” như giả định; hệ thống đọc đúng sector/block size cần hỗ trợ.
- **LBP:** có magic, ABI version, kích thước, memory descriptor version/stride, framebuffer, ACPI RSDP, kernel/modules, boot device, kernel entry and ownership/lifetime rules. Quy định các trường hợp không có framebuffer/ACPI và version mismatch. Chọn register/stack/canonical address/mapping state ở milestone thiết kế, sau đó đóng băng ABI version.
- **C++:** ở giai đoạn sớm chỉ dùng subset freestanding phù hợp; tự triển khai runtime cần thiết, không giả định libstdc++, exceptions, RTTI, dynamic initialization đã hoạt động.
- **GPU:** VirtIO-GPU 2D là đường hiển thị/resource/scanout cho **software rendering**, chưa phải guest 3D acceleration; virgl/3D đòi hỏi driver, protocol và runtime guest thích hợp, khả năng tương thích host/WSL2 cần được kiểm chứng, không hứa chắc thời điểm.
- **LamFS:** ưu tiên throughput/latency, nhưng từ ngày đầu phải có bounds checks, metadata consistency, kiểm thử mất điện/ghi hỏng cơ bản; journaling/COW/checksum có thể được chọn sau theo phân tích.
- **A/B:** gắn với phân vùng hệ thống, metadata giao dịch, bảo vệ dữ liệu người dùng và rollback; không cập nhật ESP/boot chain tùy tiện mà không có chiến lược recovery.

## 2. Chính sách milestone, trạng thái và nghiệm thu

Trạng thái hợp lệ: `PLANNED` → `IN_PLAN` → `APPROVED` → `IN_PROGRESS` → `VERIFYING` → `DONE`; hoặc `BLOCKED`. Mặc định **mọi milestone bên dưới là `PLANNED`**, ngoại trừ trạng thái ghi riêng có bằng chứng, không đánh dấu hoàn thành theo suy đoán.

**Khi lập kế hoạch trong Codex Plan Mode, mỗi milestone phải có:**
- Mục tiêu, phạm vi/ngoài phạm vi, điều kiện tiên quyết và đánh đổi.
- Những file/module/ABI bị ảnh hưởng, sơ đồ dữ liệu và ownership, kế hoạch triển khai theo bước nhỏ.
- Các lỗi có thể xảy ra, biện pháp rollback và tiêu chí dừng.
- Build/test cụ thể và *bằng chứng dự kiến* (serial marker, test assertion, exit status, screenshot nếu kiểm chứng đồ họa).
- Danh sách câu hỏi cần người dùng duyệt. **Không sửa mã nguồn trước khi có duyệt.**

**Definition of Done chung:** code/build tương ứng chạy được; test liên quan thực sự pass; không regression trên boot path đã hỗ trợ; lỗi/giới hạn được ghi minh bạch; `CHANGELOG.md`, `docs/SESSION_HANDOFF.md` và trạng thái `ROADMAP.md` được cập nhật bằng kết quả có thật. Đối với phần chỉ tài liệu, nghiệm thu là tính đầy đủ và nhất quán, không yêu cầu giả lập đã chạy.

**Ma trận kiểm thử tăng dần:** BIOS SeaBIOS và UEFI OVMF (`-serial`/log machine-readable); 1 CPU, sau đó 2/4/8 CPU khi SMP hoạt động; RAM cấu hình khác nhau; VirtIO-Blk; VirtIO-GPU 2D; Intel HDA; VirtIO-Net; hồi quy negative tests (ELF hỏng, memory map lỗi, filesystem lỗi, syscall invalid). Chỉ kích hoạt nhánh có subsystem tương ứng.

## 3. ROADMAP theo thứ tự phụ thuộc

**Cấu trúc:** Giai đoạn lớn `P` gồm các milestone `M`. Mốc triển khai thực tế nên giới hạn trong một phiên chat; nếu vượt quá thì tách thành task nhỏ được duyệt riêng. Thứ tự dưới đây là đề xuất phụ thuộc kỹ thuật, không phải lịch thời gian cố định. **Trạng thái: M00 DONE; M01 DONE; M02 DONE; M03 DONE; M04 DONE; M05 DONE; M06 DONE; M07 DONE; M08 DONE; M09 DONE; M10 DONE; M11 DONE; M12 DONE; M13 DONE; M14 DONE; M15 DONE; M16 DONE; M17 DONE; M18 PLANNED; M19 DONE; M20 DONE; M21 IN_PROGRESS; M22–M44 PLANNED.**

### P0 — Khởi tạo và nền tảng chất lượng

**M00 — Repository và hệ thống tài liệu.** **Trạng thái: DONE (2026-10-24).** Đạt tiêu chí ban đầu và phần bổ sung GitHub Public/GPL-3.0-only/Documentation CI; người dùng đã xác nhận hoàn thành, annotated tag `milestone/M00` đã được push; xem [kế hoạch](PLANS.md) và [handoff](docs/SESSION_HANDOFF.md). Chuẩn hóa root repo, `AGENTS.md`, `README.md`, `GOALS.md`, `PLANS.md`, `CHANGELOG.md`, `CONTRIBUTING.md`, `.gitignore`, tài liệu trong `docs/`, skill có ích; kiểm tra `ROADMAP.md` là tài liệu gốc do người dùng cung cấp, **không thay toàn bộ nội dung nếu chưa được cho phép**. Nghiệm thu: cấu trúc có trách nhiệm rõ, tài liệu tiếng Việt, handoff ngắn và hướng dẫn Plan Mode.

**M01 — Toolchain/reproducible build trên WSL2.** **Trạng thái: DONE (2026-10-24).** Doctor, Debug/Release sample, ELF/link provenance và SHA-256 hai source roots đã PASS tại local và Toolchain CI; người dùng đã xác nhận kết thúc M01, annotated tag `milestone/M01` đã push. Phát hiện công cụ thật, CMake/Ninja, GNU cross-compiler, NASM, Binutils, Python, QEMU, GDB, OVMF, clang-format/tidy, cppcheck; `scripts/doctor.py`, preset/profile build debug/release và tối thiểu smoke CI local. Nghiệm thu: báo phiên bản, build sample freestanding, không link nhầm thư viện host.

**M02 — QEMU harness và đo kiểm tự động.** **Trạng thái: DONE (2026-10-26).** Fixture/cleanup, launch-only BIOS/UEFI và ba workflows đã được kiểm chứng; boot skeleton SKIPPED vì chưa có image, không phải OS boot PASS; người dùng đã phê duyệt kết thúc, annotated tag `milestone/M02` đã push. `pytest` khởi chạy QEMU có timeout, nhận serial markers, phân loại hang/panic/exit, lưu logs, CTest host tests, kịch bản BIOS và UEFI skeleton; test chưa thực thi được phải `SKIP` có giải thích. Nghiệm thu: ít nhất một bài test harness tự kiểm thử kết quả pass/fail thật.

### P1 — Bootchain tự viết, hợp nhất giao thức

**M03 — Đặc tả LBP v1 + image layout GPT.** **Trạng thái: DONE (2026-10-27).** Contract/thư viện/host tests và bốn workflows đã PASS; người dùng phê duyệt kết thúc và merge PR #1; annotated tag milestone/M03 giữ SHA nghiệm thu ab7b1e4. Bằng chứng ở handoff; chưa có OS boot. Định nghĩa cấu trúc on-wire/in-memory, memory map normalization, version negotiation, kernel entry ABI, physical/virtual ownership; thiết kế Protective MBR, BIOS Boot Partition, ESP và System A/B/User Data. Nghiệm thu: tài liệu + headers + host unit tests/struct layout assertions, quyết định không còn mơ hồ.

**M04 — BIOS Stage 1.** **Trạng thái: DONE (2026-10-27).** Vòng tối ưu đã được duyệt; gate 436/440 byte và guest SeaBIOS positives/negatives PASS local. Regression local và bốn workflows SUCCESS trên commit code; người dùng đã xác nhận hoàn thành, PR #2 đã merge và annotated milestone/M04 đã push; chưa có OS boot, xem handoff. Mã NASM boot sector phù hợp GPT, tải stage tiếp từ BIOS Boot Partition, thông báo lỗi qua cổng phù hợp/serial khi có thể. Nghiệm thu: QEMU SeaBIOS chạy stage xác định, không đụng GPT metadata.

**M05 — BIOS Stage 2 và môi trường x86_64.** **Trạng thái: DONE (2026-10-28).** Kế hoạch đã được người dùng duyệt; BIOS→ELF64/LBP probe local Debug/Release và regression PASS, bốn workflows SUCCESS trên code0a0f496, người dùng đã xác nhận hoàn thành, annotated milestone/M05 giữ SHA nghiệm thu4742e2d và PR #3 đã merge; xem handoff. BIOS disk access, memory discovery (E820), A20 nếu cần, GDT, transition 16→32→64, page tables tối thiểu, kernel ELF64 load, BootInfo LBP. Nghiệm thu: cùng kernel entry, serial boot marker, kiểm tra memory map/reserved frames.

**M06 — UEFI EFI loader tự viết.** **Trạng thái: DONE (2026-10-29).** PR #4 merged; tag `milestone/M06` giữ commit nghiệm thu `b96c6ba3`; bốn workflows CI SUCCESS trên SHA nghiệm thu. OVMF thực thi loader và cùng ELF probe BIOS nhận LBP v1. PE/COFF x64 dùng Clang+LLD, firmware ABI Microsoft x64, đọc ESP/GPT/FAT32 qua UEFI, nạp ELF64, GOP/ACPI optional, memory map và `ExitBootServices()`. Xem [handoff](docs/SESSION_HANDOFF.md) và [kế hoạch](PLANS.md). PC thật/4Kn chưa kiểm chứng; M07 DONE.

**M07 — Boot parity, disk image generator và lỗi khởi động.** **Trạng thái: DONE (2026-10-29).** PR #5 merged; annotated tag `milestone/M07` trỏ commit nghiệm thu `1ece4235`; bốn workflows CI SUCCESS trên đúng SHA. Generator/checker raw GPT profile 8 GiB và boot thật SeaBIOS/OVMF trên cùng image/kernel ELF; probe xác nhận nghĩa vụ LBP v1 chung và negative ELF bị từ chối. Xem [handoff](docs/SESSION_HANDOFF.md). PC thật, 4Kn, slot B và filesystem System/User chưa thuộc nghiệm thu.

### P2 — Kernel foundation và quản lý bộ nhớ

**M08 — Kernel tối thiểu, serial panic, exception.** **Trạng thái: DONE (2026-10-29).** PR #6 merged; annotated tag `milestone/M08` trỏ commit nghiệm thu `56cbc3bd`; bốn workflows CI SUCCESS trên đúng SHA. Kernel C/NASM dùng chung ELF boot thật qua SeaBIOS/OVMF, xác thực LBP v1, cài GDT/IDT/TSS+IST, xuất panic context và nhận IRQ0/PIT self-test; `#UD`/guard `#PF` nhận đúng vector. Review ACPI span đã sửa trước nghiệm thu. OVMF ACPI-aligned guest path và PC thật ghi NOT RUN; xem [handoff](docs/SESSION_HANDOFF.md). Mục tiêu milestone: C/ASM entry, C++ freestanding runtime nhỏ khi cần, IDT/GDT/TSS đủ dùng, exception vectors, panic dump rõ; PIC bootstrap cho external IRQ cơ bản. Nghiệm thu: serial logs ổn định, cố ý gây exception nhận đúng vector, không triple fault im lặng.

**M09 — Bitmap PMM.** **Trạng thái: DONE (2026-10-29).** PR #7 merge; annotated tag `milestone/M09` trỏ commit nghiệm thu `b2ef8182`; bốn workflow SUCCESS trên đúng SHA. Bitmap 2-bit/frame trong kernel `.bss` quản lý frame 4 KiB dưới 4 GiB; chỉ `LBP_USABLE` vào pool, reservations bị loại, không reclaim LOADER/ACPI. Host/sanitizer và cùng-kernel guest boot SeaBIOS/OVMF PASS; xem [handoff](docs/SESSION_HANDOFF.md) và [quản lý bộ nhớ](docs/MEMORY_MANAGEMENT.md). Mục tiêu: parse memory map chuẩn LBP, đánh dấu vùng firmware/kernel/modules/ACPI/MMIO, cấp/trả 4KiB frames; unit tests và stress tests có kiểm tra. Nghiệm thu: không trả frame reserved, xử lý hết bộ nhớ an toàn.

**M10 — VMM và kernel address space.** **Trạng thái: DONE (2026-10-30).** PR #8 merge commit `493cd8a8e6790d0d1adbd87e7500b596698613dd`; CI bốn workflow SUCCESS trên head nghiệm thu `7e0b2f288e0497deb4a2c5fd4a50c679643643ba`. VMM paging 4 cấp tạo root CR3 mới trên BSP; hỗ trợ map/unmap/protect 4 KiB và range, scratch page-table access, W^X, NX khi khả dụng, guard và TLB invalidation. Host/sanitizer, 8 guest boots BIOS/OVMF và hồi quy M07–M09 PASS. Review đã phát hiện và sửa quyền execute khi NX không khả dụng; regression native được thêm. Annotated tag `milestone/M10` đã push, giữ nguyên trên commit ban đầu `dd6365c6c217bfc076f73ec0cb5efdcb8e05a777`; review-fix `7e0b2f2` có trong PR merge nhưng tag không di chuyển. Không đổi LBP v1 hay loader contract. PC thật, firmware ngoài QEMU, SMP, RAM trên 4 GiB, process address spaces và kernel sản phẩm NOT RUN/ngoài phạm vi. Xem [handoff](docs/SESSION_HANDOFF.md).

**M11 — Kernel heap và C++ nền tảng.** **Trạng thái: DONE (2026-10-30).** PR #9 merge commit `c1d288e7ca4fcc9f87c9eb08ba257098e6111bc8`; commit nghiệm thu `3bf87334999a794bce6dec7e8d691f5f125ff781`. Bốn workflow GitHub Actions SUCCESS trên SHA nghiệm thu; review tự động không có findings. Annotated tag `milestone/M11` object `618d9a27610e5ce73c81abad120b04872f60587b` trỏ commit nghiệm thu. M11 CTest 3/3 PASS, guest BIOS/OVMF cùng ELF PASS, hồi quy M07–M10 PASS. PC thật và firmware ngoài profile QEMU NOT RUN. Xem [kế hoạch và kết quả](PLANS.md) cùng [handoff](docs/SESSION_HANDOFF.md).

**M12 — Buddy allocator nâng cấp PMM.** **Trạng thái: DONE (2026-10-30).** PR #10 merge commit `385f835b5825528915080859156e963db15e6fd0`; acceptance commit `f3391d66eabc6b6d82a27800db6e229bb74f577b`; bốn workflow GitHub Actions SUCCESS trên acceptance SHA. Annotated tag `milestone/M12` object `b4a0ee9ed47e3e6b77a8658e5270d8af8338bffe` trỏ acceptance commit. M12 CTest 6/6 PASS trong Toolchain CI; guest BIOS/UEFI trên cùng kernel phát marker M08–M12 PASS. Buddy là backend thử nghiệm song song, bitmap vẫn mặc định; benchmark CI không cho thấy lợi thế throughput/fragmentation. PC thật và firmware ngoài profile QEMU NOT RUN. Xem [kế hoạch/kết quả](PLANS.md) và [handoff](docs/SESSION_HANDOFF.md). Nghiệm thu không yêu cầu xóa bitmap bootstrap.

### P3 — Thiết bị nền tảng, ngắt hiện đại và đồ họa QEMU sớm

**M13 — ACPI cơ bản, PCI enumeration, device framework.** **Trạng thái: DONE (2026-10-31).** PR #11 merge commit `26f3b3698f803fbcc8c8a39e7c9b4f01a631a7e5`; acceptance commit `12d3ef1ac9040f9d523bb05a82066dadcaec6ecf`; annotated tag `milestone/M13` object `ff2b1fbbae53d37af78aed566f381ac1b122f2d6` trỏ acceptance commit. Bốn GitHub Actions workflows SUCCESS trên đúng acceptance SHA. Host ASan/UBSan kiểm bảng hợp lệ và malformed; guest BIOS/UEFI cùng kernel tìm PCI functions và BAR/MMIO resources chính xác. Firmware guest không cung cấp RSDP qua LBP (`M13:ACPI:ABSENT`), do đó ACPI traversal guest **NOT RUN**; PC thật và firmware ngoài profile **NOT RUN**. Chi tiết tại [PLANS](PLANS.md) và [handoff](docs/SESSION_HANDOFF.md). Tại thời điểm đóng M13, M14 chưa bắt đầu.

**M14 — Local APIC/IOAPIC, timers.** **Trạng thái: DONE (2026-10-31).** PR #12 merge commit `8360690506b1472d3b31959f36bb3a511e76282e`; acceptance commit `f41d20df47b7e95ac9ac73027c7cd7a7395b67c5`; annotated tag `milestone/M14` object `85876228135c5f1d971a1a30687e999ce5ddab39` trỏ acceptance commit. Bốn workflow GitHub Actions SUCCESS trên acceptance SHA: Toolchain [41177278867](https://github.com/duclam2011dz/LamixOS/actions/runs/41177278867), Harness [41177278863](https://github.com/duclam2011dz/LamixOS/actions/runs/41177278863), Documentation [41177278850](https://github.com/duclam2011dz/LamixOS/actions/runs/41177278850), Boot Contract [41177278957](https://github.com/duclam2011dz/LamixOS/actions/runs/41177278957). CTest M14 3/3 PASS; BIOS SeaBIOS `pc` và UEFI OVMF `q35` cùng kernel/image 3/3 boots mỗi profile, các marker ACPI/APIC/IOAPIC/PIT reference/LAPIC timer đạt, protected hashes/cleanup đúng. Hồi quy Toolchain M05 4/4, M06 UEFI 3/3, M07 2/2, M08 2/2, M09 3/3, M10 3/3, M11 3/3, M12 6/6, M13 3/3 PASS. Automated review commit cuối không có major findings; finding hiệu chuẩn được sửa và resolved. PC thật và firmware ngoài profile QEMU **NOT RUN**. Tại thời điểm nghiệm thu M14, M15 còn PLANNED.

**M15 — Input bàn phím và chuột PS/2.** **Trạng thái: DONE (2026-10-31).** PR #13 merge commit `eba0607607545638fe1569fa129abac1b283d7d1`; acceptance commit `831748b5d65f0a88394745b20da82a56f69439c4`; annotated tag `milestone/M15` object `69aa3ebdb5c2843487b73925c8b73f96893cd2d7` trỏ acceptance commit. Bốn workflow SUCCESS trên acceptance SHA: Toolchain [41180666554](https://github.com/duclam2011dz/LamixOS/actions/runs/41180666554), Harness [41180666561](https://github.com/duclam2011dz/LamixOS/actions/runs/41180666561), Documentation [41180666589](https://github.com/duclam2011dz/LamixOS/actions/runs/41180666589), Boot Contract [41180666560](https://github.com/duclam2011dz/LamixOS/actions/runs/41180666560). M15 CTest 3/3 PASS, guest BIOS/UEFI 3/3 boots mỗi firmware; hồi quy M07–M14 và docs PASS. PC thật/firmware ngoài profile **NOT RUN**. Chi tiết tại [PLANS](PLANS.md), [TESTING](docs/TESTING.md) và [handoff](docs/SESSION_HANDOFF.md).

**M16 — Software framebuffer renderer.** **Trạng thái: DONE (2026-10-31).** PR [#14](https://github.com/duclam2011dz/LamixOS/pull/14) merge commit `e37b0b6c3f5423ae518c32e2fc325a2dd5a4a19b`; acceptance commit `09c4248dece6273295c28639090883d00a3af9c0`; annotated tag `milestone/M16` object `a66b7f9a0fbf1d52cb3df1be2b9d09e0c0862a63` trỏ acceptance commit. Bốn workflow SUCCESS trên đúng SHA: Toolchain [41197034264](https://github.com/duclam2011dz/LamixOS/actions/runs/41197034264), Harness [41197034242](https://github.com/duclam2011dz/LamixOS/actions/runs/41197034242), Documentation [41197034318](https://github.com/duclam2011dz/LamixOS/actions/runs/41197034318), Boot Contract [41197034328](https://github.com/duclam2011dz/LamixOS/actions/runs/41197034328). Automated review trên acceptance SHA không còn findings; finding VBE 3.0 pitch/masks linear đã sửa. M16 CTest 3/3 PASS, guest SeaBIOS `pc` và OVMF `q35` mỗi profile 1/1 PASS; hồi quy M07/Stage2 host/M15 host 54 PASS, 57 deselected; docs validator PASS, unittest 41/41 PASS. BIOS Stage2 core/BSS 176,128/262,144 byte; bootstrap 32 KiB. PC thật/firmware ngoài profile **NOT RUN**. Chi tiết tại [PLANS](PLANS.md), [TESTING](docs/TESTING.md) và [handoff](docs/SESSION_HANDOFF.md).

**M17 — VirtIO transport + VirtIO-GPU 2D. Trạng thái: DONE (2026-10-31).** PR [#15](https://github.com/duclam2011dz/LamixOS/pull/15) merge commit `8afa222fc493c2adbac71bb415adacc03b4510f7`; acceptance commit `292eb59f3c368c2933a7dbb0d135b89f4fb74415`; annotated tag `milestone/M17` trỏ acceptance commit. Bốn workflow SUCCESS trên acceptance SHA: Toolchain [41200982371](https://github.com/duclam2011dz/LamixOS/actions/runs/41200982371), Harness [41200982402](https://github.com/duclam2011dz/LamixOS/actions/runs/41200982402), Documentation [41200982454](https://github.com/duclam2011dz/LamixOS/actions/runs/41200982454), Boot Contract [41200982320](https://github.com/duclam2011dz/LamixOS/actions/runs/41200982320). Sau nghiệm thu, PR [#16](https://github.com/duclam2011dz/LamixOS/pull/16) sửa đường boot trực tiếp bị chặn tại M15 input self-test và thêm `scripts/m17_qemu.py`; merge commit `d290ef5ab6547b6b0228e38ed5571054dea16ecb`. Bốn workflow trên follow-up SHA `9c7b7f484be79c9b22c922ac1ff0d2b316b88e3e` đều SUCCESS: Toolchain [41206133212](https://github.com/duclam2011dz/LamixOS/actions/runs/41206133212), Harness [41206133271](https://github.com/duclam2011dz/LamixOS/actions/runs/41206133271), Documentation [41206133306](https://github.com/duclam2011dz/LamixOS/actions/runs/41206133306), Boot Contract [41206133141](https://github.com/duclam2011dz/LamixOS/actions/runs/41206133141). M17 CTest 3/3, guest BIOS/UEFI GPU và fallback 4/4; người dùng xác nhận cả hai cửa sổ QEMU hiển thị bình thường. Tag `milestone/M17` vẫn trỏ acceptance SHA, không di chuyển. PC thật/firmware ngoài profile **NOT RUN**. Xem [PLANS](PLANS.md), [TESTING](docs/TESTING.md) và [handoff](docs/SESSION_HANDOFF.md). M18 vẫn PLANNED, chưa bắt đầu.

**M18 — GPU acceleration proof-of-concept (nhánh nghiên cứu có gate).** Đánh giá virgl/virtio-gpu 3D, firmware/host QEMU/WSL2 và giao thức guest; tạo prototype nhỏ có phép đo, quyết định Go/No-go. Nghiệm thu: báo cáo kiểm chứng thực tế; nếu thiếu điều kiện thì `BLOCKED` có bằng chứng, **không tuyên bố 3D accelerated**. Có thể quay lại milestone này sau khi GUI và SDK đã trưởng thành.

### P4 — Multitasking, user mode và IPC

**M19 — Preemptive priority round-robin scheduler (BSP). Trạng thái: DONE (2026-10-31).** Thêm kernel threads BSP, context switch qua LAPIC timer, bốn mức priority/RR, aging, mutex FIFO/direct handoff và priority inheritance; M18 giữ PLANNED và không phải dependency. Acceptance commit `9a0861ec4b152e189144251ece884bc6ffd466d5`; PR [#18](https://github.com/duclam2011dz/LamixOS/pull/18) merge commit `c550210f4ba69f7181ed4a3cf0001e20059ea52a`; annotated tag `milestone/M19` object `6d055839a1dce6356eba49c0e2a22e6630708b8a` trỏ acceptance commit. Bốn workflow SUCCESS trên SHA nghiệm thu: Toolchain [41213184919](https://github.com/duclam2011dz/LamixOS/actions/runs/41213184919), Harness [41213184786](https://github.com/duclam2011dz/LamixOS/actions/runs/41213184786), Documentation [41213184903](https://github.com/duclam2011dz/LamixOS/actions/runs/41213184903), Boot Contract [41213184777](https://github.com/duclam2011dz/LamixOS/actions/runs/41213184777). Native ASan/UBSan và host tests PASS; guest BIOS/UEFI ba lần mỗi firmware PASS với preemption CPU-bound, RR, aging và mutex inheritance/handoff/wakeup; M17 guest regression 4/4 PASS; docs validator và unittest 41/41 PASS. Codex review sạch trên acceptance SHA, mọi review thread resolved. PC thật và firmware ngoài profile QEMU **NOT RUN**. Chi tiết nghiệm thu tại [PLANS](PLANS.md) và [handoff](docs/SESSION_HANDOFF.md).

**M20 — Processes, ring 3, address-space isolation. Trạng thái: DONE (2026-11-01).** PR [#20](https://github.com/duclam2011dz/LamixOS/pull/20) merge commit `88821fdd59a6341b636e37e7e9a045f536d218a8`; acceptance commit `d0b97be7566e793bc0c54810ec574903ae2ed3dc`; annotated tag `milestone/M20` object `651e6ea9b065ac9a3759399fe9e4996511a0ce55` trỏ acceptance commit. Bốn workflow CI SUCCESS trên acceptance SHA: Toolchain [41265639668](https://github.com/duclam2011dz/LamixOS/actions/runs/41265639668), Harness [41265639777](https://github.com/duclam2011dz/LamixOS/actions/runs/41265639777), Documentation [41265639726](https://github.com/duclam2011dz/LamixOS/actions/runs/41265639726), Boot Contract [41265639667](https://github.com/duclam2011dz/LamixOS/actions/runs/41265639667). Guest cùng kernel/image qua SeaBIOS `pc` và OVMF `q35` 3/3 lần mỗi firmware PASS; kiểm tra ring 3, timer scheduling, hai address space, truy cập chéo bị chặn, user fault cô lập, process còn lại tiếp tục và kill/wait/cleanup. M20 cross-build, ASan/UBSan, regressions M17/M19, docs validator và unittest 41/41 PASS. Local CMake/CTest wrapper **NOT RUN** do thiếu `cmake`; CI Toolchain chạy CTest thành công. PC thật và firmware ngoài profile QEMU **NOT RUN**. M18 vẫn PLANNED; M21 chưa bắt đầu.

**M21 — Lamix-specific syscall ABI v1. Trạng thái: IN_PROGRESS (2026-11-01).** Đang triển khai syscall calling convention, syscall table/version, return/error policy, handle validation, user-space C wrappers, `SYSCALL/SYSRET` và safe fallback. Profile M21 riêng; không đổi LBP v1 hoặc entry ABI. Local cross-build, CTest **3/3 PASS**, guest BIOS/UEFI 3/3 mỗi firmware và hồi quy M17/M19/M20 hiện PASS. GitHub CI trên acceptance SHA **NOT RUN**; M21 chưa nghiệm thu. Tiêu chí roadmap giữ nguyên: ABI integration tests và fault injection invalid args.

**M22 — Handle-based capabilities và IPC.** Object handles có rights, duplication/transfer, channels/messages, shared memory khi an toàn; quyền tối thiểu, synchronization tests. Nghiệm thu: negative permission tests, race/close/reuse tests.

**M23 — Static ELF64 loader/user runtime.** Process loader, relocations hỗ trợ theo phạm vi, arguments/environment, `crt0`, libc subset tối thiểu, executable format validation. Nghiệm thu: chạy hai user programs độc lập từ disk/initramfs, handle lỗi ELF hỏng.

**M24 — SMP: AP startup và per-CPU scheduler.** MADT AP discovery, trampoline/INIT-SIPI theo thiết kế, per-CPU stacks/TSS/interrupt state, TLB shootdown, per-CPU run queues, affinity/balancing. Nghiệm thu: QEMU 1/2/4/8 vCPU, stress concurrency không deadlock trong tiêu chí test; **không bỏ chế độ BSP-only**.

### P5 — Lưu trữ, filesystem, ứng dụng đầu tiên

**M25 — Block I/O + VirtIO-Blk.** Storage abstraction, async/sync request boundaries, đọc/ghi an toàn; kiểm thử không ghi lên disk host. Nghiệm thu: read/write/verify dữ liệu trên test image dùng riêng.

**M26 — VFS + tmpfs + FAT32.** Path resolution, vnode/file handles, mount semantics, caching ban đầu, FAT32 reader rồi writer, kiểm tra FAT chain và lỗi metadata. Nghiệm thu: create/read/write/delete/rename/mount tests, persistence qua reboot, không nhầm ESP với user data.

**M27 — System services và shell/terminal native.** Init/service lifecycle, console, filesystem utilities, log viewer, command shell căn bản; IPC/service permissions. Nghiệm thu: chạy lệnh, thao tác file, xử lý lỗi và reboot sạch trong QEMU.

**M28 — User-space desktop compositor + window manager.** Shared buffers/IPC, windows/surfaces, mouse/keyboard routing, frame scheduling, transparency/shadows/blur theo tầng, shell/taskbar/launcher, dark neon Gaming Glass và performance mode. Nghiệm thu: nhiều cửa sổ, resize/focus/input đúng, UI responsive trên software renderer/VirtIO-GPU 2D.

### P6 — Gaming native 2D và âm thanh

**M29 — Lamix native 2D SDK v0 + Pong.** C/C++ app ABI, event loop, render surfaces/sprites, timing, keyboard/input, API docs/build template. Nghiệm thu: Pong chạy native ngoài kernel, điều khiển được và frame timing có đo đạc.

**M30 — 2D framework & game library mở rộng.** Sprites, tilemap, font/text, animation, collision, asset loader; Snake + Breakout + platformer nhỏ. Nghiệm thu: nhiều game khởi chạy/cài đặt/thoát ổn định; regression không gây treo desktop.

**M31 — Intel HDA driver + user-space audio server.** Device initialization, DMA/audio buffers, PCM stereo output, mixing, volume, timing; Lamix Audio API. Nghiệm thu: QEMU Intel HDA phát tone/test waveform, kiểm tra buffer underrun, game có âm thanh.

**M32 — `lpm` offline + ứng dụng game.** Manifest, versions, dependency metadata, install/uninstall, file/path validation, atomic staging & rollback, danh mục game local. Nghiệm thu: cài và gỡ Pong/Snake/Breakout từ gói offline, phát hiện package hỏng.

**M33 — Dynamic linking và tối ưu runtime (khi cần).** PIC/shared libraries, dynamic linker, symbol/version policy, ASLR, runtime C++ được hỗ trợ có chọn lọc, benchmarks. Nghiệm thu: native apps liên kết động hoạt động, ABI compatibility và negative tests.

### P7 — Networking (cố ý đặt ở phase muộn)

**M34 — VirtIO-Net + Ethernet/ARP/IPv4/ICMP.** DMA/rings, link management, packet parsing, IPv4 routing tối thiểu, checksum, ICMP ping; network permissions. Nghiệm thu: giao tiếp trong QEMU virtual network, packet malformed không làm hỏng kernel.

**M35 — UDP/TCP + DHCP/DNS + socket API.** Socket handles, buffering, retransmission, connection states, non-blocking I/O cơ bản; DHCP lease, DNS client. Nghiệm thu: UDP echo/TCP echo, resolver test, disconnect/retry tests dưới QEMU.

**M36 — Gaming networking + `lpm` online.** Async sockets, multiplayer proof-of-concept native game, package repo metadata, signature verification/trust, download/install atomic. Nghiệm thu: 2 QEMU guests chơi demo multiplayer đơn giản; online packages được xác minh và rollback khi hỏng.

**M37 — Network mở rộng.** IPv6/NDP, Intel E1000 driver, TLS sau khi crypto primitives và entropy được đánh giá; firewall và resilience. Nghiệm thu: IPv6 connectivity / E1000 QEMU tests, protocol fuzz tests; tính năng chưa sẵn sàng phải ghi `BLOCKED`.

### P8 — LamFS, updates, trải nghiệm phát hành

**M38 — LamFS hiệu năng cao.** Tài liệu on-disk format v1, benchmarks, caching, block allocator, metadata consistency, crash recovery tối thiểu, migrator/backup test. Nghiệm thu: đo throughput/latency so FAT32, filesystem check/fuzz và reboot/corruption tests; không hy sinh an toàn dữ liệu cơ bản.

**M39 — A/B updates + rollback.** System A/B metadata, slot selection, image verification, health markers, atomic commit, failure rollback, user data separation, BIOS/UEFI boot compatibility. Nghiệm thu: giả lập update thành công và thất bại, tự quay về slot cũ, dữ liệu người dùng không đổi.

**M40 — GUI/graphics acceleration trưởng thành.** Đánh giá kết quả M18; phát triển con đường GPU acceleration khả thi (VirtIO-GPU 3D/virgl nếu được kiểm chứng), compositor/game 2D hưởng lợi nhưng có software fallback. Nghiệm thu: benchmark + functional tests và rollback fallback, không đưa ra cam kết 3D chung chung.

**M41 — Gaming platform mở rộng.** Gamepad/input mapping, audio latency, fullscreen, frame pacing, assets, saves, developer SDK/examples; bổ sung nhiều game native khác thể loại: puzzle, racing 2D, arcade, platformer, strategy mini. Nghiệm thu: thư viện game đa dạng chạy ổn định, ít crash, benchmark latency/fps trong QEMU.

### P9 — Ổn định, bảo mật và phát hành playable

**M42 — Security/hardening và fuzz.** Handle rights audit, syscall/ELF/VFS/network fuzzing, W^X/NX/ASLR tùy khả năng, random/entropy, review DMA/driver boundary, user-driver isolation. Nghiệm thu: negative/security tests và bản ghi rủi ro còn lại.

**M43 — Reliability, performance & compatibility matrix.** Stress/soak tests, memory pressure, leak tests, desktop/game crash recovery, multi-vCPU, BIOS/UEFI, storage corruption, boot recovery, release profiles. Nghiệm thu: công bố minh bạch test matrix, uptime và game test evidence; không tự bịa số liệu.

**M44 — Lamix OS Playable Release.** Installer/image distribution, tài liệu sử dụng, SDK, app/game catalog, changelog, release notes, reproducible artifacts; giới hạn phần cứng hỗ trợ được công bố. Nghiệm thu: người dùng boot BIOS/UEFI trên QEMU profile hỗ trợ, đăng nhập/khởi chạy desktop, cài/chạy nhiều game native, lưu dữ liệu, âm thanh, networking căn bản nếu đủ điều kiện; update/rollback được kiểm thử.

## 4. Định nghĩa chất lượng "Playable" (không chỉ là GUI khởi động)

Một bản Lamix OS được gọi là **Playable** khi có bằng chứng:

- BIOS và UEFI đều boot từ image được hỗ trợ; kernel panic/exception thể hiện được trong logs.
- Process isolation, threads, syscalls/IPC, storage/VFS và file persistence hoạt động dưới test.
- Desktop Gaming Glass responsive, window/input/gamepad nếu đã cam kết hoạt động; có software rendering fallback.
- SDK và nhiều game native **thuộc các thể loại khác nhau** khởi chạy, đóng/mở, lưu tiến độ được; Pong chỉ là game khởi đầu.
- Audio Intel HDA hoạt động trên profile QEMU hỗ trợ; network native cơ bản và package manager làm đúng phạm vi phát hành.
- A/B update có đường recovery chứng minh được, không làm mất User Data trong test.
- Có regression matrix, crash/soak test và danh sách hạn chế công khai; không hứa hỗ trợ mọi PC/GPU trước khi có driver thực tế.

## 5. Quy tắc tiết kiệm usage, tài liệu và chuyển session

- Root `AGENTS.md` **ngắn**, mô tả quy tắc cố định và đường dẫn tham chiếu, không nhét toàn roadmap vào đó. `src/AGENTS.md`, `boot/AGENTS.md`, `tests/AGENTS.md` **chỉ tạo nếu thật sự cần hướng dẫn theo thư mục**.
- `README.md`: hướng dẫn nhanh; `GOALS.md`: mục tiêu/phạm vi; `ROADMAP.md`: milestone, phụ thuộc và trạng thái; `PLANS.md`: kế hoạch milestone đang duyệt/triển khai, không sao chép lại roadmap; `CHANGELOG.md`: lịch sử thay đổi **đã xảy ra**.
- Trong `docs/`: `ARCHITECTURE.md`, `CONTEXT.md`, `CONVENTIONS.md`, `RULES.md`, `TESTING.md`, `SECURITY.md`, `DECISIONS.md` (dạng ADR), `BOOT_PROTOCOL.md`, `MEMORY_MANAGEMENT.md`, `GRAPHICS.md`, `API.md`, `TROUBLESHOOTING.md`, `SESSION_HANDOFF.md`; `CONTRIBUTING.md` tại root. Có thể hoãn/tối giản tài liệu chưa có dữ kiện, ghi `Chưa triển khai` thay vì viết giả tưởng như đã hoàn thành.
- `.agents/skills/<skill>/SKILL.md`: skill chuyên dụng, có YAML metadata và workflow tái dùng; khởi tạo **chỉ skill thật sự có việc** (os-development, bootloader, kernel-debugging, driver, regression testing). Không tạo skill rỗng chỉ để đủ danh sách.
- Đầu phiên: đọc `AGENTS.md`, `docs/SESSION_HANDOFF.md`, **đúng đoạn liên quan** trong `ROADMAP.md`, `PLANS.md` khi cần và file kỹ thuật có liên quan; không load toàn bộ repository nếu nhiệm vụ hẹp. Có thể dùng `git status`, diff, symbol search để xác minh thực tế.
- Cuối milestone/phiên có thay đổi: cập nhật `SESSION_HANDOFF.md` tối đa khoảng 1–2 trang: milestone, quyết định, file sửa, lệnh test + kết quả **thật**, blocker, bước tiếp theo; `CHANGELOG.md` chỉ ghi thay đổi đã xong, `ROADMAP.md` chỉ đổi trạng thái có bằng chứng. Không tạo hàng loạt log/snapshot trùng lặp.
- **Không tự sửa/phá kiến trúc đã chốt.** Nếu cần thay đổi ABI, disk layout, order dependencies hoặc chiến lược test, đưa vào Plan Mode để người dùng duyệt, ghi ADR rồi cập nhật roadmap.

## 6. Gợi ý bắt đầu sau khi nhận tài liệu

1. Người dùng đặt `ROADMAP.md` này ở root workspace.
2. Mở **Codex Plan Mode**, cung cấp Master Prompt để Codex **khảo sát repo và lập kế hoạch riêng cho M00**; chưa yêu cầu xây hết OS.
3. Người dùng duyệt kế hoạch M00; Codex mới tạo các file MD còn lại theo phạm vi được duyệt.
4. Tiếp tục **M01** bằng một **phiên Plan Mode mới**; sau mỗi milestone thực hiện bàn giao context.

## 7. Tham chiếu kỹ thuật cần tra cứu khi cần

- Intel® 64 and IA-32 Architectures Software Developer’s Manuals (registers, paging, interrupts, SMP).
- UEFI Specification (PE/COFF boot, Boot Services, GetMemoryMap/ExitBootServices).
- ACPI Specification (RSDP, MADT, CPU/APIC tables).
- PCI/PCI Express và VirtIO specifications (queues, memory ordering, device features).
- ELF System V ABI + x86-64 psABI (program headers, relocations, calling convention).
- FAT32 filesystem specification, GPT/UEFI partition specification.
- QEMU documentation về system emulation, OVMF, VirtIO devices, VirtIO-GPU 2D/3D.

**Kết thúc bản ROADMAP v1.0. Đây là kế hoạch được đề xuất, không phải báo cáo các tính năng đã hoàn thành.**
