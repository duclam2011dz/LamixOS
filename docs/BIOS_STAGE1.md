# BIOS Stage1 — M04

Stage1 có guest smoke test trên QEMU pc/SeaBIOS, không phải Lamix OS boot. M05 BIOS Stage2 và M06 UEFI EFI loader đều dùng [ELF/LBP kernel probe](BIOS_STAGE2.md); M06 đã DONE theo [handoff](SESSION_HANDOFF.md). Contract GPT/LBP M03 giữ nguyên; [image layout](IMAGE_LAYOUT.md) và [boot protocol](BOOT_PROTOCOL.md) là nguồn tương ứng.

## Vùng mã và phạm vi

[stage1.asm](../boot/bios/stage1.asm) dùng BITS 16, ORG 7c00, flat binary NASM. Mã/data hữu ích hiện 436 byte, zero padding 4 byte, artifact 440 byte. Assertion `times 440 - ($ - $$) db 0` vẫn bắt buộc; host test chèn thêm bytes vào bản copy để xác nhận assembler thật sự từ chối vượt budget. Bytes 440–511 do fixture builder tạo Protective MBR, không thuộc source Stage1.

Stage1 chỉ INT13 EDD 512, hard-disk DL>=80h; capacity phải <2^32 sectors, từ chối high dword, không truncate hoặc CHS fallback. Đọc GPT header LBA1 và array LBA2 (128 entries x128 byte), so GUID đầy đủ 16 byte và dùng partition đầu tiên khớp. Không guest CRC, backup comparison, overlap validation toàn bảng hoặc phát hiện BIOS GUID trùng; Stage2 M05 bổ sung phần đó, không coi Stage1 là parser dữ liệu không đáng tin cậy hoàn chỉnh.

| Vùng | Physical range |
|---|---|
| EDD parameters/GPT header | 0600–07ff |
| Stack | SS=0, SP=7c00; tăng xuống, DAP chỉ sống trong helper |
| Stage1 mã/data | 7c00–7db7 |
| Entry array | 8000–bfff |
| Cửa sổ stage tiếp | 10000–17fff,64 sectors/32 KiB |

Không self-modifying code, code/data overlap, helper ngoài PMBR, post-MBR gap hoặc disk writes. Drive byte và diagnostic placeholder là data riêng trong bản MBR resident ở RAM; ghi chúng không ghi disk hoặc sửa instruction bytes. Stage1 không ghi trực tiếp IVT/BDA.

## Checks và audit source/listing

| Invariant | Source labels | Bằng chứng |
|---|---|---|
| Chuẩn hóa CS, segments, stack và DF | start | Far jump0000, initialization trước BIOS calls; probe kiểm state nhận được |
| COM1 115200/8N1, UART IRQ tắt | start | DLAB/DLL/DLM/LCR/IER explicit; bounded putc |
| Boot drive đúng trước EDD | start | Reload DL sau UART init; sửa lỗi draft539 để DL=F9h |
| AH41 CF/BX/capability và AH48 CF/bps/high-capacity | start/unsupported | Mỗi check giữ nguyên; lỗi D; EDD/4Kn failures chưa có guest injection riêng |
| Header signature/revision/size và table LBA/high/count/stride | header_expected/profile_ok | Hai REP CMPSW 16 byte tại offsets 0–15 và 72–87 ; 69 guest cases gồm mutation từng byte 32-byte prefix |
| Duyệt tối đa 128 entries, GUID 16 byte | scan/next_entry/bios_guid | REP counter/pointer push/pop; relocated fixture đặt BIOS entry tại127, GUID mutation 16 positions |
| Start/end high=0, start>=34, end<=N-34, end>=start, end-start>=63 | found/bad_gpt | OR high words, unsigned checks, SUB/CF trước size check; lỗi G |
| EBP capacity không bị helper phá | found/read_sectors | PUSHAD trước dùng BP làm frame, POPAD phục hồi EBP; boundary tests kiểm upper bounds |
| Retry tối đa3, reset giữa attempts, DAP inputs mới | read_sectors/retry/read_failed | DI counter 3, packet tạo lại, BIOS CF hoặc count mismatch không đi success; EIO thật nhận R |
| DAP 16 byte căn 4, packet cleanup | retry/read_failed/read_done | Caller SP7c00→CALL7bfe→PUSH DS7bfc→PUSHAD7bdc→DAP7bcc; ADD SP 16 ở cả success/failure |
| Handoff đầy đủ | found/read_done | Probe kiểm CS/SP/DS/ES/SS/flags/DL/ESI/EDI trước khi sửa state |
| Diagnostics giữ nguyên | fail/print_message/putc/halt | M04:FAIL LF reason LF; UART polling tối đa 65535 vòng/char, sau đó halt hữu hạn |

Không dựa vào giá trị GPR ban đầu trừ DL firmware cung cấp. BIOS EDD theo calling convention bảo toàn các GPR ngoài AX/flags; helper không cam kết chịu firmware vi phạm ABI đó. EAX input nằm trong PUSHAD frame, không dùng AH status làm LBA retry. ES:BX/CX được giữ theo BIOS ABI; DAP high LBA luôn0 ở mỗi attempt. CF được kiểm tra trước các phép cleanup SP đổi flags. Success POPAD/POP DS/RET trả đúng stack caller.

REP header dùng DS=ES=0, DF=0; bảng expected chứa đủ high bytes, không rút thành low-word checks. Mismatch của GUID có thể dừng REP sớm, nhưng pointer SI và outer counter CX luôn được phục hồi trước bước entry tiếp. ESI/EDI handoff được gán full32 bit sau khi chọn partition.

## ABI Stage1 tới stage tiếp

Stage1 đọc 64 sectors từ partition start tới1000:0000 rồi far jump, không return. Success: CS:IP=1000:0000; DS/ES/SS=0; SP=7c00; IF=1/DF=0; DL=boot drive, ESI=start và EDI=end inclusive. Các registers khác không xác định, A20 chưa thay đổi. Đây không phải kernel LBP entry ABI.

COM1 errors: D=EDD/profile, G=GPT shape/bounds, N=no BIOS Boot GUID, R=read failure. [Probe](../tests/bios/stage_probe.asm) chỉ phát M04:PASS sau kiểm ABI, từng pattern của sectors 1–63 và trailer 16 byte cuối. Probe failure phát M04:FAIL rồi P; không thực hiện E820/A20/CPU transition/ELF load.

## Fixtures, harness và bằng chứng

[Builder test](../tests/bios/image_fixture.py) tạo mới sparse raw image 8 GiB, dùng metadata encoder M03 thuần tách thành helper. Chỉ metadata, Stage1, probe và sentinel data test; không mkfs/mount/loop device hoặc filesystem. Output chỉ trong build/out/tmp, xb từ chối ghi đè/symlink/device đã có. Canonical fixture được M03 validator kiểm; relocated/exact-window là GPT tổng hợp kiểm bounds, không thay profile sản phẩm.

Relocated fixture chuyển BIOS Boot đến vùng khác và entry 127, điều chỉnh User Data để không overlap; chứng minh tìm GUID và LBA thật. Images cấp phát thực tế dưới 1 MiB; tests so hashes đúng các extents được ghi, không hash toàn 8 GiB hoặc tuyên bố đã scan toàn disk.

Harness purpose=bios-stage1 chỉ nhận BIOS+image, không launch-only; result có backend=qemu/mode=bios-stage1. Default Config positional fields và M02 mode/skip semantics giữ nguyên. CLI thêm `--purpose bios-stage1`; fault_sector chỉ là test Config field. QEMU luôn snapshot; TMPDIR riêng mỗi run để snapshot không phụ thuộc /var/tmp chỉ đọc trong sandbox. Không dump environment.

EIO test dùng blkdebug ở sector payload+31, errno 5/read_aio/once=off, rerror=report. Negative tests assert PANIC và reason dự kiến, không đổi guest JSON thành PASS. case.json ghi expected/actual, serial và protected hashes để đọc artifact đúng nghĩa. OS boot skeleton BIOS/UEFI vẫn SKIPPED vì không dùng fixture Stage1 làm OS image mặc định.

Baseline gate 539 byte được giữ ignored với source hash/listing/logs. Vòng đã duyệt đo A 494, B 467, C 436; NASM dùng -Ox mặc định, không nới gate. Các phép đo local này không thay thế kiểm chứng guest. Kiểm tra và CI hiện tại xem [handoff](SESSION_HANDOFF.md), [TESTING](TESTING.md#bios-stage1-m04).

## Giới hạn

Chưa kiểm thử firmware thiếu EDD, logical sectors 4Kn, drive DL khác profile, BIOS vi phạm register ABI hoặc CF=0/short-count giả lập. Probe/negative tests trên SeaBIOS không chứng minh tương thích PC thật, actual kernel paging, UEFI boot hoặc A/B recovery. Không tạo tag M04 trước xác nhận hoàn thành riêng.

Nguồn đối chiếu: [NASM flat binary](https://nasm.us/doc/nasm09.html), [SeaBIOS disk services](https://raw.githubusercontent.com/qemu/seabios/master/src/disk.c), [QEMU 6.2 blkdebug](https://raw.githubusercontent.com/qemu/qemu/v6.2.0/docs/devel/blkdebug.txt). Retry/window/diagnostics/memory map là policy Lamix đã duyệt; không nhập mã firmware vào repository.
