# GPT image layout — profile Lamix v1

M03 đặc tả layout và mô hình metadata host. M07 đã thêm generator/checker FAT32/GPT cho image probe; đây chưa phải image OS sản phẩm hoặc filesystem System/User. M05 có fixture ESP FAT32/kernel probe test-only; M06 fixture này thêm EFI application để boot OVMF. M04 có sparse fixture riêng cho Stage1/probe, xem [BIOS_STAGE1](BIOS_STAGE1.md). [GPT headers](../include/lamix/boot/gpt.h) dùng byte arrays alignment=1, assertions mọi offset; [mô hình host](../tests/boot/gpt_model.py) không file/disk I/O và được dùng làm metadata validator cho M07. Trạng thái M07 và bằng chứng ở [handoff](SESSION_HANDOFF.md).

## Metadata theo chuẩn và policy

Theo [UEFI GPT](https://uefi.org/specs/UEFI/2.10/05_GUID_Partition_Table_Format.html), LBA0 chứa Protective MBR, primary header tại LBA1, backup header tại LBA cuối; partitions nằm trong usable range, không đè hai entry arrays/headers. CRC32 chuẩn IEEE qua zlib: tính header với CRC field zero; entry-array CRC bao phủ entries, không bao phủ padding ngoài array.

Policy Lamix: revision 0x00010000, header 92 byte, 128 entries x 128 byte = 16384 byte mỗi array, phần còn lại header block zero. Không hybrid MBR. Primary array tại LBA2, backup array ngay trước backup header. Với `N=disk_bytes/block_size`, `A=16384/block_size`:

- Primary header=1; primary entries=2..1+A.
- FirstUsable=2+A; LastUsable=N-A-2 (inclusive).
- Backup entries=N-A-1..N-2; backup header=N-1.
- Block size mô hình: 512 hoặc 4096; disk_bytes dương, chia hết block_size và <2^64. FirstUsable tương ứng 34/6.
- Profile boot ban đầu **chỉ 512**, 4Kn BIOS/UEFI boot chưa cam kết.

PMBR: bytes0–439 dành cho stage1 M04, M03 fixture để zero; bytes440–445 zero, một partition record tại446, ba records còn lại zero, signature55 aa tại510. Protective record BootIndicator=0, StartingCHS=00 02 00, typeEE, EndingCHS=ff ff ff, StartingLBA=1, SizeInLBA=min(N-1,0xffffffff). Với block4096, bytes512..4095 zero. Dùng saturated EndingCHS theo profile LBA bootstrap; không hỗ trợ CHS boot. Stage1 không được ghi GPT hoặc post-MBR gap.

## Partitions và profile tham chiếu

| Index | Name | Type GUID | Start | Size |
|---:|---|---|---:|---:|
| 1 | BIOS Boot | 21686148-6449-6e6f-744e-656564454649 | 1 MiB | 2 MiB |
| 2 | ESP | c12a7328-f81f-11d2-ba4b-00a0c93ec93b | 3 MiB | 256 MiB |
| 3 | System A | b2c2410b-b210-56ec-bdc3-589072b4fe3d | 259 MiB | 2048 MiB |
| 4 | System B | b2c2410b-b210-56ec-bdc3-589072b4fe3d | 2307 MiB | 2048 MiB |
| 5 | User Data | 881eed6f-b4b9-5e1b-b0bf-c0a4f1eb2dd9 | 4355 MiB | Đến LastUsable inclusive |

Default disk=8 GiB. Start offsets chia block_size thành start LBA; partitions1–4 kết thúc ngay trước start kế. User Data tối thiểu1 MiB; disk nhỏ hơn tổng yêu cầu bị từ chối, không co System slots. Mọi start căn1 MiB; cuối User Data không ép căn1 MiB. Profile kích thước khác chỉ thay disk size trong mô hình M03; thay partition sizes cần profile/layout revision được duyệt.

GUID BIOS Boot là convention từ [GNU](https://www.gnu.org/software/grub/manual/grub/grub.html), không phải yêu cầu dùng GRUB. ESP GUID do UEFI quy định. Lamix System/User GUID là UUIDv5 namespace URL chuẩn, name chính xác `https://github.com/duclam2011dz/LamixOS/system` và `https://github.com/duclam2011dz/LamixOS/user-data` (UTF-8, phân biệt case, không slash cuối). Đây là type GUID, không phải disk/partition identity.

Disk GUID và 5 unique partition GUIDs phải không zero, đôi một khác nhau; A/B phân biệt bằng unique GUID và index/slot, không chỉ tên. M07 quyết định seed để tạo instance GUIDs tái lập; M03 fixtures dùng identity giả cố định và không cấp GUID cho disk thật. Attributes tất cả zero; names là UTF-16LE đúng bảng, zero padded đến72 byte. Entries6–128 hoàn toàn zero. GUID wire theo UUID.bytes_le: 3 field đầu little-endian,8 byte cuối giữ thứ tự.

## Boot payload và A/B

ESP FAT32 dự kiến có `EFI/BOOT/BOOTX64.EFI`, `EFI/LAMIX/A/KERNEL.ELF`, `EFI/LAMIX/B/KERNEL.ELF` và modules của từng slot trong cùng thư mục. M06 EFI loader đọc slot A tường minh, không tự boot B nếu A lỗi; BIOS M05 dùng cùng kernel ELF probe. Profile M07 hiện chỉ đặt kernel probe ở A để kiểm parity; modules và bundle sản phẩm chưa thuộc acceptance hiện tại.

BIOS Boot Partition là raw stage storage, không filesystem. BIOS Stage1 phải tìm partition thay vì đọc gap; không ghi đè partition table. Generator M07 chỉ đưa EFI application và kernel probe vào slot A; B/User Data để trống và chưa có filesystem format được đóng băng. ESP chỉ chứa boot payload, không coi là User Data. A/B layout hiện không chứng minh atomic updates, health check hoặc rollback; metadata giao dịch/recovery và bảo vệ ESP khi update thuộc M39.

## Host validation và giới hạn

Mô hình nhận riêng PMBR, hai header blocks và hai entry arrays trong memory. Kiểm tra block/size, CRC, primary/backup mirror fields, exact array locations, usable bounds, GUID identity/type/encoding, partition order/bounds/no overlap, alignment, names/attributes và unused entries. Từ chối metadata hỏng, không repair hoặc ghi disk.

Fixtures chỉ tạo metadata tổng hợp khoảng vài chục KiB để kiểm tra assertions, không ghép raw image hoặc tạo executable boot code. Không gọi mkfs, mount, loop device, parted, dd hay QEMU với image. Host tests không chứng minh firmware tìm ESP hoặc BIOS đọc đúng sectors; M04–M07 kiểm chứng thật.

## Fixtures M05

Builder tests/bios_stage2/stage2_fixture.py tạo mới sparse8 GiB, metadata đúng M03, Stage1/Stage2 trong BIOS Boot Partition, ESP FAT32 tối thiểu và kernel ELF trong slot A. Không format System/User, không mount/device/overwrite hoặc cài filesystem driver ghi. Thông tin thực thi và giới hạn ở [BIOS_STAGE2](BIOS_STAGE2.md); generator dùng cho người dùng/UEFI parity vẫn thuộc M07.
