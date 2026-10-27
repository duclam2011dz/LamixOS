# Lamix Boot Protocol v1.0

Contract M03 được duyệt ngày 2026-10-26. Headers và thư viện validation/normalization đã triển khai; BIOS Stage1 M04 có smoke test riêng; Stage2/UEFI loader/kernel và kernel handoff **chưa triển khai**. Kết quả kiểm chứng ở [handoff](SESSION_HANDOFF.md); không coi host tests là OS boot.

## Encoding và cấu trúc

Nguồn headers: [lbp.h](../include/lamix/boot/lbp.h), [contract.h](../include/lamix/boot/contract.h). Mọi field offset/size và alignment nằm trong [bảng layout](LBP_LAYOUT.md); assertions C11/C++17 kiểm tra từng field. `uint8_t/16_t/32_t/64_t`, little-endian, không native pointers/size_t/enums/bitfields trên wire. Không dùng packed structs; padding tường minh. Parser đọc từng byte, không cast buffer chưa xác minh.

BootInfo là blob liên tục, physical base căn 8, dưới 4 GiB, tối đa 1 MiB và 256 records. Header 64 byte: magic ASCII `LAMIXLBP`, major=1, minor=0, header_size=64, total_size gồm toàn blob; required/provided feature masks, records_offset=64, record_count, reserved=0 và self_phys. Numeric register magic là `0x50424c58494d414c` (cùng 8 byte little-endian).

Record header 16 byte: type:u32, flags:u32, size:u64. Bit flags 0 là REQUIRED, bit khác phải zero. Size gồm header, payload và zero padding đến bội số 8. Records liên tục từ records_offset; tổng size phải kết thúc đúng total_size, không overlap hoặc trailing garbage. Header extension có thể làm records_offset lớn hơn header_size; khoảng trống giữa hai giá trị phải zero. Caller có thể truyền buffer lớn hơn total_size, nhưng phần ngoài blob không thuộc contract.

Địa chỉ tài nguyên ngoài blob là physical u64. Offset tên modules là offset từ đầu blob, không phải pointer. Producer ghi zero mọi reserved/padding/descriptor extension; consumer v1 kiểm tra zero padding đã biết. Với minor cao hơn, consumer bỏ qua descriptor extensions/trailing record extensions sau prefix đã biết; reserved fields cũ vẫn phải zero. Các field chỉ địa chỉ virtual được đặt tên rõ trong bảng layout. API native của thư viện khác với wire ABI.

## Records và nội dung

| Type | Record | Payload sau record header |
|---:|---|---|
| 1 | Memory map | version:u32=1, stride:u32>=32 (bội 8), count:u64; `lbp_memory[]` |
| 2 | Kernel | entry_virtual:u64; version/stride/count; `lbp_segment[]` |
| 3 | Reservations | version/stride/count; `lbp_reservation[]` |
| 4 | Entry context | cr3_phys, stack_phys, stack_size, rsp, guard_virtual: năm u64; version/stride/count; `lbp_segment[]` mappings |
| 5 | Boot device | source:u32 (1 BIOS/2 UEFI), block_size:u32=512; disk/ESP/System GUID (mỗi 16 byte); slot:u32 (1 A/2 B), reserved:u32=0 |
| 6 | Modules | version/stride/count; `lbp_module[]`; tên UTF-8 liên tục theo thứ tự descriptors, padding cuối zero |
| 7 | Framebuffer | physical/length:u64; width/height/pitch/bpp:u32; red/green/blue/reserved masks:u32 |
| 8 | ACPI | RSDP physical:u64; length:u32; revision:u32 |

Types 1–5 bắt buộc, REQUIRED=1, mỗi type đúng một record, các bảng không rỗng. Types 6–8 tùy chọn, tối đa một record/type; Modules có thể count=0. Trùng known type là CONFLICT. Unknown optional record được bỏ qua sau bounds validation; unknown REQUIRED record trả UNSUPPORTED. Version descriptor khác 1 bị từ chối. Bảng tối đa 4096 entries; stride mở rộng được chấp nhận; phần extension phải zero với minor=0, được bỏ qua với minor cao hơn. Không truyền EFI handles, BIOS callbacks hoặc runtime service pointers.

`lbp_segment` là physical:u64, virtual:u64, length:u64, permissions:u32, reserved:u32. Kernel segments và mappings có base/length căn 4 KiB, physical không zero và kết thúc không vượt 4 GiB. Permissions: READ=1, WRITE=2, EXECUTE=4; READ bắt buộc, WRITE+EXECUTE không được đồng thời. Các kernel segments không chồng physical hoặc virtual; mappings không chồng virtual và không có writable/executable physical aliases. Mọi mapping phải thuộc một reservation, không được tự map MMIO/firmware ranges bằng entry contract này.

Modules descriptor: physical/length/name_offset:u64; name_length/reserved:u32. Physical bytes nằm trong MODULE reservation, không chồng module khác; tên không NUL terminator, không rỗng, UTF-8 hợp lệ, cấm NUL/control characters. Tên nằm sau toàn bộ descriptors trong chính record Modules, liên tục theo thứ tự descriptors; tổng tên và padding không vượt record. Module physical range không bắt buộc page aligned, nhưng reservation bao phủ nó phải page aligned.

Framebuffer v1 hỗ trợ packed 32 bpp, pitch bội 4 và >= width*4, pitch*height <= length, dimensions không zero, range không overflow. RGB masks khác zero, liên tục và không giao nhau; reserved_mask zero hoặc liên tục, không giao RGB. Range không chồng reservations hoặc memory usable/loader reclaim. Không bắt buộc identity map framebuffer. Pixel masks là lựa chọn profile Lamix, không cam kết mọi mode firmware.

ACPI bootstrap hỗ trợ RSDP revision 0/length 20 và revision 2/length 36, physical căn 16. Kiểm tra signature, revision, legacy checksum; revision 2 kiểm tra length, extended checksum và reserved bytes. Bytes được caller cung cấp qua span physical→host bytes; thiếu span trả TRUNCATED, không dereference physical address. RSDP không được nằm trong usable/loader reclaim hoặc chồng reservations. Duyệt XSDT/RSDT và ACPI tables thuộc M13.

GUID dùng encoding GPT (ba field đầu little-endian, 8 byte cuối theo thứ tự UUID); disk/ESP/System GUID không zero, không trùng nhau. Phát hiện GUID bằng firmware/disk thuộc M05/M06; M03 không kiểm tra chúng với disk thật.

## Negotiation và compatibility

ELF PT_NOTE owner `LAMIX\0`, namesz=6, type=1, descsz=32, note alignment 4. Descriptor `lbp_note`: magic `LAMIXREQ`, major:u16=1, minimum_minor:u16=0, reserved:u32=0, required_features:u64, optional_features:u64. Note descriptor không thay ELF note envelope; loader phải kiểm tra envelope và đúng một note ở M05/M06. API M03 chỉ kiểm tra descriptor, không parse ELF hoặc phát hiện note trùng trong ELF.

Features: bit 0 FRAMEBUFFER, bit 1 ACPI, bit 2 NX. Required/optional masks của note phải rời nhau. Unknown required features, major khác 1 hoặc minimum_minor>0 bị UNSUPPORTED; unknown optional features được bỏ qua. Loader lấy available features, kiểm tra note, phát provided_features phản ánh kết quả thật và required_features bằng yêu cầu kernel. Kernel vẫn gọi validator khi entry; required phải là subset provided và known features. Không có fallback version im lặng.

Provided FRAMEBUFFER/ACPI phải khớp sự hiện diện record tương ứng; NX chỉ báo khả năng/state do loader xác minh, host không kiểm tra CR/MSR. Serial bootstrap cho phép thiếu framebuffer/ACPI nếu kernel note không yêu cầu. Loader báo lỗi nếu record yêu cầu không hợp lệ.

Minor tương lai chỉ thêm dữ liệu tùy chọn, không đổi nghĩa field cũ; consumer v1 nhận minor cao hơn với prefix tương thích và không có yêu cầu lạ. Unknown optional feature bits không cho phép sử dụng tính năng chưa hiểu. Major mới cần negotiation mới và phê duyệt riêng. Header/record additions không được làm thay đổi offset fields đã có.

## Memory map normalization

`lbp_memory` 32 byte: base:u64, length:u64, type:u32, flags:u32, firmware_attributes:u64. Flags: bit0 UEFI attributes, bit1 NONVOLATILE. Khi bit0=0, attributes giữ E820 extended attributes (với 20-byte entry coi là 1). Type: 1 USABLE, 2 RESERVED, 3 BAD, 4 ACPI_RECLAIM, 5 ACPI_NVS, 6 LOADER_RECLAIM, 7 RUNTIME. Unknown raw types trở thành RESERVED.

Firmware adapters là hàm thuần trên buffers:

- E820 stride 20 hoặc 24, buffer phải chia hết stride. Với 24 byte, bỏ entry valid bit0=0; bit1 nonvolatile không được biến thành usable. Zero-length bỏ qua.
- UEFI descriptor version=1, stride>=40 và bội 8; đọc PhysicalStart offset8, NumberOfPages offset24, Attributes offset32. Padding offset4 phải zero; physical căn page. Dùng stride firmware, không giả định sizeof descriptor. Pages*4096 và range end được kiểm tra overflow.
- UEFI LoaderCode/Data, BootServicesCode/Data thành LOADER_RECLAIM; Conventional thành USABLE; RuntimeCode/Data thành RUNTIME; ACPI reclaim/NVS và unusable giữ loại tương ứng. Reserved/MMIO/Persistent/Unaccepted/unknown giữ RESERVED. RUNTIME attribute luôn bảo vệ range; NV attribute 0x8000 và E820 nonvolatile không cho cấp phát như RAM thường; USABLE/LOADER_RECLAIM/ACPI_RECLAIM có NV đều hạ về RESERVED.

Chuẩn hóa dùng ranges `[base, base+length)`, sort theo base, không overlap. Không thêm RAM cho holes. Các endpoints được tách theo floor/ceil 4 KiB; một page chỉ usable khi toàn page thuộc RAM phù hợp. Partial pages thành RESERVED. Adjacent ranges chỉ merge khi type/flags/attributes giống nhau.

Thứ tự bảo thủ khi overlap: BAD > RUNTIME > RESERVED > ACPI_NVS > ACPI_RECLAIM > LOADER_RECLAIM > USABLE. Attributes/flags khác nhau tại cùng interval: ít nhất RESERVED, flags/attributes output=0; BAD/RUNTIME vẫn được giữ. Không suy đoán cacheability. Thứ tự input không thay kết quả.

Normalizer kiểm tra provenance trước overlay; validator BootInfo không thể khôi phục firmware map gốc từ map đã overlay. Reservations không overlap, page aligned, thuộc toàn bộ USABLE/LOADER_RECLAIM trước overlay; nếu gặp hole/BAD/MMIO/firmware reserved/ACPI/runtime/nonvolatile thì CONFLICT. Sau overlay, reservation thành RESERVED và có record riêng để phân biệt ownership. ACPI reclaim chỉ thu hồi sau khi đọc/copy tables; NVS/runtime không tự reclaim. LOADER_RECLAIM chỉ thu hồi sau khi kernel thay toàn bộ tài nguyên loader và không còn references.

## Entry ABI và mappings

Đây là nghĩa vụ producer/consumer cho M05–M07; chưa có boot test:

- BSP x86_64 ring0/long mode, `CR0.PE=PG=WP=1`, `CR4.PAE=1`, LA57=0; paging 4 cấp. `EFER.LME/LMA=1`; NXE bật nếu provided NX=1, mọi mapping không executable được đặt NX khi khả dụng.
- Kernel ELF64 ET_EXEC, virtual segments/entry trong `[0xffffffff80000000, 0xffffffffffffffff]`, range không wrap; chưa KASLR. Kernel load segments là page-disjoint để không tạo RWX page.
- `RDI=self_phys`, `RSI=0x50424c58494d414c`; tham số integer theo System V. Các general registers khác không xác định trừ RSP. Không return; loader chuyển bằng jump sau khi tạo slot giả 8 byte zero tại đỉnh stack.
- Stack identity mapped RW, ít nhất 64 KiB; `RSP=stack_phys+stack_size-8`, `RSP%16=8`. Guard page ngay dưới stack không map ở bất kỳ mapping virtual nào. Không dùng red zone/SIMD/FPU trước kernel initialization; compile entry bằng `-mno-red-zone -mgeneral-regs-only`.
- IF=0, DF=0. GDT/IDT hợp lệ, CS 64-bit ring0, data selectors hợp lệ; không hard-code selector values. Loader giữ descriptors và handlers fatal hợp lệ cho tới khi kernel thay thế. Không bảo đảm SIMD/FPU, TR, FS/GS state: kernel phải thiết lập trước khi sử dụng và không dùng TLS sớm.
- CR3 physical căn 4 KiB, page tables có TABLES reservations. Entry mappings liệt kê trong record; không giả định full identity/direct map. Kernel segments map đúng virtual/permissions, BootInfo page coverage identity RO (mọi alias của blob cũng chỉ READ), stack identity RW. Page tables/loader tables/code cần tồn tại phải được reservations bảo vệ, transition resources có mappings cần thiết do loader lập và kiểm chứng M07.
- Physical allocations kernel/modules/BootInfo/stack/tables không zero và kết thúc <=4 GiB. RAM cao hơn vẫn có trong map. Framebuffer/ACPI physical có thể cao hơn, chỉ truy cập sau khi map phù hợp.

## Ownership và thư viện

Tất cả ownership chuyển từ loader sang kernel tại entry, nhưng release conditions vẫn ràng buộc:

| Purpose | Release | Lifetime |
|---:|---:|---|
| 1 KERNEL | 0 NEVER | Kernel quyết định ở subsystem tương lai |
| 2 MODULE | 0 NEVER | Không tự reclaim chỉ vì parse xong BootInfo |
| 3 BOOTINFO | 1 COPIED | Blob bất biến đến khi copy/parse và bỏ mọi view |
| 4 STACK | 2 REPLACED | Sau khi chuyển stack |
| 5 TABLES | 2 REPLACED | Sau khi thay page tables/GDT/IDT liên quan |
| 6 LOADER | 2 REPLACED | Sau khi không còn code/data/handler references |

Thư viện [contract.c](../lib/boot/contract.c), [memory.c](../lib/boot/memory.c) không heap, global mutable state, libc hoặc firmware APIs. Mọi input/scratch/output buffers do caller sở hữu; caller bảo đảm mapped/readable/writable theo dung lượng đã khai báo và không alias nhau. Các spans không overlap; pointer validation ở API chỉ kiểm tra NULL/range metadata, không thể xác minh page tables host.

`lbp_validate_note` kiểm tra note descriptor với available_features. `lbp_validate` kiểm tra blob và các spans, chỉ publish `lbp_view` khi thành công. `lbp_record_at` lấy record offset từ validated view; caller phải giữ blob bất biến và còn sống. Không dùng view tự chế để bỏ validation.

`lbp_decode_e820/uefi` có hai lượt, chỉ ghi output sau khi toàn input và capacity hợp lệ. `lbp_normalize` dùng scratch tối thiểu `4*(input_count+reservation_count)+2` descriptors; mỗi input count tối đa 4096. Scratch query qua capacity thiếu trả BUFFER_TOO_SMALL và needed=scratch descriptors; khi scratch đủ, needed=output descriptors thực tế. Query output bằng output=NULL/capacity=0, scratch vẫn cần có. Input rỗng hợp lệ, output count=0. Scratch có thể đổi khi lỗi; output và view không publish một phần. Khi thiếu scratch, normalization chưa thể hoàn tất kiểm tra semantic reservations; caller phải cấp scratch rồi gọi lại. Nếu thành công, caller dùng needed entries, phần output còn lại không xác định.

Status numeric: OK=0, INVALID=1, TRUNCATED=2, OVERFLOW=3, UNSUPPORTED=4, CONFLICT=5, BUFFER_TOO_SMALL=6. Overflow/wrong format được phát hiện trước output copy. Normalization có độ phức tạp bậc hai, phù hợp giới hạn bootstrap, không là allocator runtime.

## Nguồn chuẩn và giới hạn kiểm chứng

- [UEFI GPT](https://uefi.org/specs/UEFI/2.10/05_GUID_Partition_Table_Format.html): on-disk metadata; [image layout Lamix](IMAGE_LAYOUT.md) ghi policy riêng.
- [UEFI Boot Services, nguồn chính thức](https://raw.githubusercontent.com/UEFI/UEFI-Specification-Release/main/source/07_Services_Boot_Services.rst): GetMemoryMap stride/map key và ExitBootServices. M06 phải dùng map hiện hành, xử lý retry đúng chuẩn, sau exit không gọi Boot Services; normalization không thay thế quy trình đó.
- [ACPI System Address Map](https://uefi.org/specs/ACPI/6.6/15_System_Address_Map_Interfaces.html): E820 và loại memory.
- [Intel SDM](https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html): CPU/paging; [x86-64 psABI](https://gitlab.com/x86-psABIs/x86-64-ABI): quy ước gọi tham chiếu. Entry profile/register choices là contract Lamix riêng.

Các bản mirror HTML/PDF có thể trả 403/404; không coi chúng là bằng chứng đã đọc hết CPU/psABI specification. Host assertions và mutation tests không chứng minh CPU state, firmware execution, ownership enforcement hoặc OS boot. Những điều đó phải kiểm chứng thật tại M05–M07.
