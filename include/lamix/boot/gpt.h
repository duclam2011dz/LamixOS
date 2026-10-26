/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef LAMIX_BOOT_GPT_H
#define LAMIX_BOOT_GPT_H
#include "lbp.h"
#define LAMIX_GPT_BIOS_TYPE "21686148-6449-6e6f-744e-656564454649"
#define LAMIX_GPT_ESP_TYPE "c12a7328-f81f-11d2-ba4b-00a0c93ec93b"
#define LAMIX_GPT_SYSTEM_TYPE "b2c2410b-b210-56ec-bdc3-589072b4fe3d"
#define LAMIX_GPT_USER_TYPE "881eed6f-b4b9-5e1b-b0bf-c0a4f1eb2dd9"
#define LAMIX_GPT_ENTRY_COUNT 128u
#define LAMIX_GPT_ENTRY_SIZE 128u
#define LAMIX_GPT_HEADER_SIZE 92u
#define LAMIX_PARTITION_ALIGNMENT UINT64_C(1048576)
/* Byte arrays giữ wire alignment=1, không dùng pragma pack/compiler extensions.
 */
typedef struct lamix_gpt_header {
  uint8_t signature[8], revision[4], header_size[4], crc32[4], reserved[4];
  uint8_t current_lba[8], backup_lba[8], first_usable[8], last_usable[8];
  uint8_t disk_guid[16], entries_lba[8], entry_count[4], entry_size[4],
      entries_crc32[4];
} lamix_gpt_header;
typedef struct lamix_gpt_partition {
  uint8_t type_guid[16], unique_guid[16], first_lba[8], last_lba[8],
      attributes[8], name_utf16le[72];
} lamix_gpt_partition;
LBP_ASSERT(sizeof(lamix_gpt_header) == 92, "GPT header");
LBP_ASSERT(LBP_ALIGNOF(lamix_gpt_header) == 1, "GPT header alignment");
LBP_ASSERT(sizeof(lamix_gpt_partition) == 128, "GPT partition");
LBP_ASSERT(LBP_ALIGNOF(lamix_gpt_partition) == 1, "GPT partition alignment");
LBP_ASSERT(offsetof(lamix_gpt_header, signature) == 0, "GPT signature");
LBP_ASSERT(offsetof(lamix_gpt_header, revision) == 8, "GPT revision");
LBP_ASSERT(offsetof(lamix_gpt_header, header_size) == 12, "GPT size");
LBP_ASSERT(offsetof(lamix_gpt_header, crc32) == 16, "GPT CRC");
LBP_ASSERT(offsetof(lamix_gpt_header, reserved) == 20, "GPT reserved");
LBP_ASSERT(offsetof(lamix_gpt_header, current_lba) == 24, "GPT current");
LBP_ASSERT(offsetof(lamix_gpt_header, backup_lba) == 32, "GPT backup");
LBP_ASSERT(offsetof(lamix_gpt_header, first_usable) == 40, "GPT first");
LBP_ASSERT(offsetof(lamix_gpt_header, last_usable) == 48, "GPT last");
LBP_ASSERT(offsetof(lamix_gpt_header, disk_guid) == 56, "GPT GUID");
LBP_ASSERT(offsetof(lamix_gpt_header, entries_lba) == 72, "GPT entries");
LBP_ASSERT(offsetof(lamix_gpt_header, entry_count) == 80, "GPT count");
LBP_ASSERT(offsetof(lamix_gpt_header, entry_size) == 84, "GPT stride");
LBP_ASSERT(offsetof(lamix_gpt_header, entries_crc32) == 88, "GPT array CRC");
LBP_ASSERT(offsetof(lamix_gpt_partition, type_guid) == 0, "partition type");
LBP_ASSERT(offsetof(lamix_gpt_partition, unique_guid) == 16, "partition GUID");
LBP_ASSERT(offsetof(lamix_gpt_partition, first_lba) == 32, "partition first");
LBP_ASSERT(offsetof(lamix_gpt_partition, last_lba) == 40, "partition last");
LBP_ASSERT(offsetof(lamix_gpt_partition, attributes) == 48, "partition attrs");
LBP_ASSERT(offsetof(lamix_gpt_partition, name_utf16le) == 56, "partition name");
#endif
