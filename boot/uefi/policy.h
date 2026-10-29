/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef LAMIX_UEFI_POLICY_H
#define LAMIX_UEFI_POLICY_H
#include "lamix/boot/lbp.h"
#include <stdint.h>

#define LAMIX_EFI_INVALID_PARAMETER UINT64_C(0x8000000000000002)
#define LAMIX_EFI_MAP_CAPACITY_MAX (2u * 1024u * 1024u)

static inline int lamix_uefi_pixel_mask(uint32_t mask) {
  if (!mask)
    return 0;
  while (!(mask & 1u))
    mask >>= 1;
  return (mask & (mask + 1u)) == 0;
}

static inline int lamix_uefi_framebuffer(uint32_t width, uint32_t height,
                                         uint32_t format,
                                         const uint32_t masks[4],
                                         uint32_t pixels_per_scanline,
                                         uint64_t base, uint64_t length,
                                         lbp_framebuffer *output) {
  uint32_t red = 0, green = 0, blue = 0, reserved = 0;
  if (!output || !width || !height || pixels_per_scanline < width || !base ||
      (base & 4095) || (length & 4095) || base >= (UINT64_C(1) << 52) ||
      length > (UINT64_C(1) << 52) - base || format > 2)
    return 0;
  if (format == 0) {
    red = 0x000000ff;
    green = 0x0000ff00;
    blue = 0x00ff0000;
    reserved = 0xff000000;
  } else if (format == 1) {
    red = 0x00ff0000;
    green = 0x0000ff00;
    blue = 0x000000ff;
    reserved = 0xff000000;
  } else if (masks) {
    red = masks[0];
    green = masks[1];
    blue = masks[2];
    reserved = masks[3];
  } else {
    return 0;
  }
  uint64_t pitch = (uint64_t)pixels_per_scanline * 4;
  if (pitch > UINT32_MAX || pitch * height > length ||
      !lamix_uefi_pixel_mask(red) || !lamix_uefi_pixel_mask(green) ||
      !lamix_uefi_pixel_mask(blue) ||
      (reserved && !lamix_uefi_pixel_mask(reserved)) || (red & green) ||
      (red & blue) || (green & blue) || (reserved & (red | green | blue)))
    return 0;
  *output = (lbp_framebuffer){{LBP_FRAMEBUFFER, 0, sizeof(lbp_framebuffer)},
                              base,
                              length,
                              width,
                              height,
                              (uint32_t)pitch,
                              32,
                              red,
                              green,
                              blue,
                              reserved};
  return 1;
}

static inline int lamix_efi_retry_exit(uint64_t status, uint32_t retries) {
  return status == LAMIX_EFI_INVALID_PARAMETER && retries == 0;
}

static inline int lamix_efi_grown_capacity(uint64_t current, uint64_t required,
                                           uint64_t descriptor_size,
                                           uint32_t growths,
                                           uint64_t *capacity) {
  if (!capacity || growths || required <= current ||
      required > LAMIX_EFI_MAP_CAPACITY_MAX)
    return 0;
  uint64_t slack = descriptor_size >= 40 && descriptor_size <= 4096
                       ? descriptor_size * 16
                       : 1024;
  if (required > LAMIX_EFI_MAP_CAPACITY_MAX - slack)
    return 0;
  uint64_t wanted = required + slack;
  if (wanted > UINT64_MAX - 4095)
    return 0;
  wanted = (wanted + 4095) & ~UINT64_C(4095);
  if (wanted > LAMIX_EFI_MAP_CAPACITY_MAX || wanted <= current)
    return 0;
  *capacity = wanted;
  return 1;
}
#endif
