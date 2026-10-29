# BIOS Stage2 — M05

Triển khai theo kế hoạch đã duyệt ngày 2026-10-28. Stage2 BIOS đã có guest probe; trạng thái local/CI chính xác ở [handoff](SESSION_HANDOFF.md). Không phải kernel sản phẩm M08. EFI loader M06 dùng chung ELF/LBP và kernel probe; M06 đã DONE. Generator/checker và boot parity M07 hiện VERIFYING. [LBP](BOOT_PROTOCOL.md) và [GPT profile](IMAGE_LAYOUT.md) M03 giữ nguyên; Stage1 M04 giữ hash/440 byte/ABI.

## Build và storage

Source tại [boot/bios/stage2](../boot/bios/stage2/); CMake suite tại [tests/bios_stage2](../tests/bios_stage2/). Core C32 compile bằng cross GCC -m32 -malign-double; tất cả translation units dùng cùng flags. Gate C/C++32/64 giữ assertions LBP, ELF không runtime/undefined symbols. Helpers division unsigned 64-bit không / hoặc %, 64 vòng hữu hạn; host oracle và vectors chạy thật trong core32 kiểm ba helper ABI. Không link libgcc64.

Stage1 tải 64 sectors đầu BIOS Boot Partition vào1000:0000. Bootstrap đúng32768 byte; core nằm tại relative LBA64, load0x20000, entry từ header, memory gồm BSS không vượt256 KiB/0x60000. Assembler chặn code/data tĩnh vượt offset0x2000 để giữ E820 scratch; linker ASSERT chặn core memory. Không dùng gap, đổi Stage1, sửa instruction bytes hoặc disk writes.

Header64 byte tại bootstrap offset32704, little-endian:

| Offset | Trường |
|---:|---|
| 0 | Magic8 byte LAMIXS2 và NUL |
| 8,10 | version u16=1, header size u16=64 |
| 12 | core relative LBA u32=64 |
| 16,20 | core file/memory sizes u32 |
| 24,28 | load physical u32=0x20000, entry offset u32 |
| 32,36 | IEEE CRC32 core file, header |
| 40–63 | Reserved zero |

Header CRC tính với field36 zero. Kiểm header/size/entry/bounds/overflow trước read; core CRC phải đạt trước execution, BSS/padding được zero. CRC không phải authentication. Header là giao diện nội bộ Stage2 v1, không thay LBP. Probe producer link ELF bình thường rồi zero riêng p_paddr các PT_LOAD, không sửa virtual/segment bytes hoặc tắt linker checks; loader bắt buộc kiểm p_paddr=0.

## Firmware, bộ nhớ thấp và transition

| Physical range | Lifetime và mục đích |
|---|---|
| 0x5000–0x5fff | State/thunk/DAP; không dùng IVT/BDA làm scratch |
| 0x6000–0x7bff | Stack bootstrap/thunk/core32 |
| 0x8000–0xffff | Bounce buffer EDD32 KiB |
| 0x10000–0x17fff | Bootstrap resident, gồm raw E820 tạm12000–137ff và tokens13800–177ff |
| 0x18000–0x1ffff | E820 sau khi vùng đích được kiểm RAM an toàn |
| 0x20000–0x5ffff | C32 core/BSS, dùng theo linker size thực |

Lưu drive/start/end ngay entry. Thu E820 qua buffer trong cửa sổ Stage1 đã tải; max256 descriptors và4096 calls; CF/signature/size20 hoặc24, token lặp/overflow đều bị chặn. Entry20 được gán attributes1, không truncate map hoặc dùng E801 để đoán RAM. Kiểm conventional/EBDA bằng BDA chỉ là giới hạn bổ sung; E820 mới là nguồn USABLE. Mọi overlapping reserved/NV range thắng RAM cho các low pages cần dùng. Chỉ copy map sang18000 và tải core khi kiểm tra đạt.

A20 kiểm alias có save/restore bytes tại5000/105000, xác minh RAM vùng thử trước. Nếu chưa bật: INT15 AX2401, rồi port92 bảo vệ reset bit; kiểm lại, fail đóng, không 8042 wait vô hạn.

Thunk C32→16 protected→real→32 dùng GDT/IDT phù hợp; BIOS IVT chỉ được nạp khi chuẩn bị real mode. Mask NMI trong cửa sổ đổi mode/table, khôi phục sau IDT hợp lệ; IF tắt trong core, BIOS được phép dùng IRQ trong real mode. DAP mới mỗi attempt, request tối đa64 sectors, CF/count mismatch fail, tối đa3 attempts/reset. Không dựa vào unreal mode: CRC/BSS advance segment khi offset16 wrap; raw E820 đọc qua ES:offset trong64 KiB. Không dùng DS=0 với physical offset32 vượt64 KiB ở real mode. Firmware vi phạm BIOS register ABI chưa được chứng minh chịu đựng.

Allocator loader first-fit pages từ normalized USABLE, dưới4 GiB và từ1 MiB trở lên; ghi reservation trước khi dùng. Không cấp low1 MiB, holes/firmware/ACPI/runtime/nonvolatile. BootInfo1 MiB, stack64 KiB có guard4 KiB, pool page tables512 KiB và emergency stack8 KiB là giới hạn bootstrap, không phải kernel allocator. Low RAM được bảo vệ trong map cuối; reservations chỉ áp dụng vùng loader thật sự sở hữu, normalization xác minh provenance.

GDT/IDT fatal có cho protected32 và long64; TSS/IST emergency stack có reservation. Paging4 cấp/4 KiB, không full identity/huge pages. Kernel higher-half map đúng permissions, không identity alias kernel; BootInfo identity RO, stack RW/guard absent. Chỉ map core/stack/tables cần tồn tại; bootstrap không cần map khi kernel entry. NXE/NX dùng khi CPU hỗ trợ, CR0.WP bật, LA57 tắt. Không BIOS calls sau paging/handoff.

## Parsers và API dùng chung

[loader.h](../include/lamix/boot/loader.h) khai báo API native, không wire ABI; implementation tại [loader.c](../lib/boot/loader.c). Caller giữ reader/context/input/output/scratch còn sống và không alias. read_at phải trả thành công chỉ khi đọc đủ bytes, không firmware APIs trong parser. Các output structs chỉ publish sau validation thành công; scratch có thể đổi khi lỗi. Không heap/global mutable state trong parser.

- lamix_gpt_open: scratch16384 byte; PMBR, headers/arrays CRC và mirror, disk/partition GUIDs, bounds/overlap, năm roles/names/order/offsets đúng M03; đối chiếu BIOS start/end Stage1 bàn giao. Không repair GPT. Disk khác profile bị từ chối; relocation M04 vẫn là fixture tổng hợp riêng.
- lamix_fat_open/lamix_fat_read: sector512, clusters power-of-two tối đa32 KiB, FAT32 cluster count/BPB/bounds/root; active FAT hoặc so mirrored entries đã dùng. Chain loops/bad/free/reserved/out-of-range/premature EOC bị chặn. Directories và file fragmented được hỗ trợ; lookup ASCII8.3 EFI/LAMIX/A/KERNEL.ELF, bỏ LFN/deleted/volume entries, từ chối target trùng/sai loại. Không fallback B, enumerate modules hoặc ghi filesystem.
- lamix_elf_inspect: ELF64 little-endian/x86_64/ET_EXEC, max128 PHDR/32 LOAD, file16 MiB, tổng page-rounded memory64 MiB. PT_LOAD page-disjoint, p_paddr0, higher-half, entry executable, bounds/overflow/W^X/alignment; chỉ publish load plan sau validation toàn bộ. lamix_elf_load nhận allocator callback trả physical/writable mapping; cấp physical liên tục từng segment, copy file và zero BSS/padding. Plan phải từ inspect và file giữ bất biến; output segments chỉ publish khi tất cả thành công, pages đã cấp vẫn do caller sở hữu khi lỗi. Từ chối INTERP/DYNAMIC/TLS/types chưa hỗ trợ; NULL/PHDR/NOTE/GNU_STACK không executable được xử lý theo profile.
- PT_NOTE envelope được duyệt đầy đủ, đúng một owner LAMIX NUL/type1/descriptor32, dùng validator M03. Framebuffer/ACPI required bị UNSUPPORTED; NX required thiếu bị từ chối. Không đổi wire structs hoặc feature bits M03.

## Kernel probe và kết quả

Probe tại tests/bios_stage2/probe.asm/probe.c/linker script; C64 -mcmodel=kernel, no-red-zone/general-regs-only. ASM chụp entry state trước C. Probe không có nhánh firmware-specific; cùng ELF dùng cho BIOS và UEFI, phát marker M05:PASS hoặc M06:PASS theo Boot Device. Physical table addresses được resolve qua mapping records trước khi dereference, không mặc định direct map; ACPI tạm ánh xạ scratch rồi gỡ trước mapping audit.

Kiểm magic/registers/flags/CR0/3/4/EFER/selectors/GDT descriptor, stack alignment/sentinel, LBP validator, initialized data/BSS, actual page translations/permissions/NX, guard và số leaf mappings đúng records. Loader phát MEMORY/RESERVE/MAP reports bằng serial và HANDOFF trước jump; chỉ kernel probe sau toàn bộ checks phát M05:PASS. Failure có M05:FAIL và nhóm S2/CPU/E820/A20/DISK/GPT/FAT/ELF/NOTE/MEMORY/LBP/ENTRY/EXCEPTION. ISR64 báo vector hex; RO/guard fault probes nhận vector0e. Polling serial hữu hạn.

Tham khảo lệnh, CI và giới hạn ở [TESTING](TESTING.md#bios-stage2-m05). Fixtures chỉ test-only sparse image mới, không mount/loop/device/overwrite. E820/A20 adapters và LBP/map corruption variants có macro test-only rõ; không coi chúng là lỗi firmware thật đã kiểm chứng. Guest negatives giữ PANIC trong JSON, pytest PASS chỉ nghĩa khớp expected.

Nguồn kỹ thuật: [ACPI E820](https://uefi.org/specs/ACPI/6.6/15_System_Address_Map_Interfaces.html), [UEFI FAT/path](https://uefi.org/specs/UEFI/2.10/13_Protocols_Media_Access.html), [ELF gABI](https://www.sco.com/developers/gabi/latest/ch5.pheader.html), [Intel SDM](https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html). Policy/limits/layout nội bộ Stage2 là lựa chọn Lamix được duyệt. Không nhập mã firmware hoặc compiler runtime bên thứ ba.

## Review segment bounds

Review sau CI đầu 68cbd48 phát hiện offset32 real-mode có thể phụ thuộc hidden segment state. Đã chuyển sang segment:offset chuẩn; test adapter inject-segments nạp tường minh các segment limits64 KiB qua protected16 rồi trở lại real mode trước và sau mỗi BIOS call. Fixture large-core zero-extend file core đến70000 byte (memory budget giữ nguyên), tính CRC lại để kiểm crossing64 KiB; large-core-crc làm hỏng byte cuối phải bị chặn. Các fixture này chỉ kiểm bootstrap, không thay compiler artifact/kernel ABI. CI68cbd48 là bằng chứng lịch sử, không thay nghiệm thu HEAD đã sửa.
