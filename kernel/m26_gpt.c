/* SPDX-License-Identifier: GPL-3.0-only */
#include "m26_gpt.h"
#include "lamix/boot/loader.h"

static uint8_t entries[16384];
static int read_block(void *context, uint64_t offset, void *bytes,
                      uint32_t size) {
  block_device *d = context;
  if (!d || offset % 512 || size % 512 || offset / 512 > d->capacity_blocks ||
      size / 512 > d->capacity_blocks - offset / 512)
    return 0;
  block_request r;
  block_request_init(&r, BLOCK_READ, offset / 512, size / 512, bytes, 0, 0);
  return block_io_sync(d, &r, 10000000) == BLOCK_OK;
}
vfs_status m26_gpt_user_extent(block_device *d, uint64_t *start,
                               uint32_t *count) {
  if (!d || !start || !count || d->capacity_blocks > UINT32_MAX)
    return VFS_INVALID;
  lamix_reader r = {d, read_block, d->capacity_blocks * 512};
  lamix_disk disk;
  if (lamix_gpt_open(r, 2048, 6143, entries, sizeof(entries), &disk) != LBP_OK)
    return VFS_CORRUPT;
  static const uint8_t type[16] = {0x6f, 0xed, 0x1e, 0x88, 0xb9, 0xb4,
                                   0x1b, 0x5e, 0xb0, 0xbf, 0xc0, 0xa4,
                                   0xf1, 0xeb, 0x2d, 0xd9};
  const uint8_t *e = entries + 4 * 128;
  uint64_t first = 0, last = 0;
  for (unsigned i = 0; i < 8; i++) {
    first |= (uint64_t)e[32 + i] << (8 * i);
    last |= (uint64_t)e[40 + i] << (8 * i);
  }
  uint64_t esp_first = disk.esp_start,
           esp_last = esp_first + disk.esp_sectors - 1;
  for (unsigned i = 0; i < 16; i++)
    if (e[i] != type[i])
      return VFS_CORRUPT;
  if (first > last || last >= d->capacity_blocks ||
      (first <= esp_last && esp_first <= last) || last - first + 1 > UINT32_MAX)
    return VFS_CORRUPT;
  *start = first;
  *count = (uint32_t)(last - first + 1);
  return VFS_OK;
}
