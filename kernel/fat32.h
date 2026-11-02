/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef LAMIX_KERNEL_FAT32_H
#define LAMIX_KERNEL_FAT32_H
#include "block.h"
#include "vfs.h"
#include <stdint.h>

typedef struct fat32 {
  block_device *device;
  vfs_memory memory;
  uint64_t start_lba;
  uint32_t sectors, fat_sectors, data_start, clusters, root_cluster, spc,
      reserved;
  uint32_t active_fat, fat_count;
  uint8_t mirrored, mounted;
  uint8_t *sector;
} fat32;

vfs_status fat32_mount(fat32 *, block_device *, uint64_t, uint32_t,
                       const vfs_memory *);
extern const vfs_ops fat32_operations;
#endif
