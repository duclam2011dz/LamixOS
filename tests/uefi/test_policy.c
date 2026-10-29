/* SPDX-License-Identifier: GPL-3.0-only */
#include "policy.h"

int main(void) {
  uint32_t bit_masks[4] = {0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000};
  lbp_framebuffer fb = {0};
  if (!lamix_uefi_framebuffer(640, 480, 0, 0, 640, 0xe0000000, 640 * 480 * 4,
                              &fb) ||
      fb.red_mask != 0x000000ff || fb.blue_mask != 0x00ff0000 ||
      fb.reserved_mask != 0xff000000)
    return 1;
  if (!lamix_uefi_framebuffer(640, 480, 1, 0, 800, 0xe0000000, 800 * 480 * 4,
                              &fb) ||
      fb.red_mask != 0x00ff0000 || fb.blue_mask != 0x000000ff)
    return 2;
  if (!lamix_uefi_framebuffer(640, 480, 2, bit_masks, 640, 0x200000000,
                              640 * 480 * 4, &fb) ||
      fb.physical != 0x200000000)
    return 3;
  fb.width = 123;
  if (lamix_uefi_framebuffer(640, 480, 3, bit_masks, 640, 0xe0000000,
                             640 * 480 * 4, &fb) ||
      fb.width != 123 ||
      lamix_uefi_framebuffer(640, 480, 2, bit_masks, 640, 0xe0000000,
                             640 * 480 * 4 - 4096, &fb))
    return 4;
  uint32_t overlapping[4] = {0x000000ff, 0x000000f0, 0x00ff0000, 0};
  if (lamix_uefi_framebuffer(640, 480, 2, overlapping, 640, 0xe0000000,
                             640 * 480 * 4, &fb))
    return 5;
  uint64_t capacity = 0;
  if (!lamix_efi_grown_capacity(1024 * 1024, 1200 * 1024, 48, 0, &capacity) ||
      capacity < 1200 * 1024 || capacity % 4096 ||
      lamix_efi_grown_capacity(capacity, capacity + 4096, 48, 1, &capacity) ||
      lamix_efi_grown_capacity(1024 * 1024, 2 * 1024 * 1024, 4096, 0,
                               &capacity) ||
      lamix_efi_grown_capacity(UINT64_MAX - 4095, UINT64_MAX, 48, 0, &capacity))
    return 6;
  if (!lamix_efi_retry_exit(LAMIX_EFI_INVALID_PARAMETER, 0) ||
      lamix_efi_retry_exit(LAMIX_EFI_INVALID_PARAMETER, 1) ||
      lamix_efi_retry_exit(0, 0) || lamix_efi_retry_exit(UINT64_MAX, 0))
    return 7;
  return 0;
}
