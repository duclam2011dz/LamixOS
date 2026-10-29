/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef LAMIX_BOOT_LOADER_H
#define LAMIX_BOOT_LOADER_H
#include "contract.h"
#ifdef __cplusplus
extern "C" {
#endif
/* API native M05; các structs dưới đây không phải on-wire format. */
typedef int (*lamix_read_at)(void *context, uint64_t offset, void *bytes,
                             uint32_t size);
typedef struct lamix_reader {
  void *context;
  lamix_read_at read;
  uint64_t size;
} lamix_reader;
typedef struct lamix_disk {
  lamix_reader reader;
  uint32_t bios_start, bios_end, esp_start, esp_sectors;
  uint8_t disk_guid[16], esp_guid[16], system_guid[16];
} lamix_disk;
typedef struct lamix_fat {
  const lamix_disk *disk;
  uint32_t fat_start, fat_sectors, data_start, clusters, root;
  uint32_t sectors_per_cluster, fat_count, active_fat, mirrored;
  uint32_t file_cluster, file_size;
} lamix_fat;
typedef struct lamix_elf_segment {
  uint64_t offset, virtual_address, file_size, memory_size;
  uint32_t permissions, reserved;
} lamix_elf_segment;
typedef struct lamix_elf_plan {
  uint64_t entry, required_features;
  uint32_t count, reserved;
  lamix_elf_segment segments[32];
} lamix_elf_plan;
/* Callback trả physical pages và writable mapping; allocations do caller sở
 * hữu, kể cả khi bước sau lỗi. Plan chỉ từ inspect, giữ nguyên cùng file bất
 * biến. */
typedef int (*lamix_alloc_pages)(void *context, uint32_t bytes,
                                 uint64_t *physical, void **writable);
lbp_status lamix_elf_load(lamix_reader file, const lamix_elf_plan *plan,
                          lamix_alloc_pages allocate, void *context,
                          lbp_segment *segments, uint32_t capacity);
uint32_t lamix_crc32(const void *bytes, uint32_t size);
lbp_status lamix_gpt_open(lamix_reader reader, uint32_t bios_start,
                          uint32_t bios_end, uint8_t *array_scratch,
                          uint32_t capacity, lamix_disk *output);
/* UEFI dùng cùng GPT checks nhưng nhận extent ESP từ device path, không từ
 * Stage1. Output chứa disk/ESP/System A GUID; không thay đổi gate BIOS. */
lbp_status lamix_gpt_open_uefi(lamix_reader reader, uint32_t esp_start,
                               uint32_t esp_sectors, uint8_t *array_scratch,
                               uint32_t capacity, lamix_disk *output);
lbp_status lamix_fat_open(const lamix_disk *disk, lamix_fat *output);
int lamix_fat_read(void *context, uint64_t offset, void *bytes, uint32_t size);
lbp_status lamix_elf_inspect(lamix_reader file, uint64_t features,
                             lamix_elf_plan *output);
#ifdef __cplusplus
}
#endif
#endif
