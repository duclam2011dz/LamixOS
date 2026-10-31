/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef LAMIX_BOOT_ACPI_RSDP_H
#define LAMIX_BOOT_ACPI_RSDP_H
#include <stddef.h>
#include <stdint.h>

static inline int lamix_rsdp_validate(const void *candidate, size_t available,
                                     uint32_t *length, uint32_t *revision) {
  const uint8_t *p = candidate;
  if (!p || !length || !revision || ((uintptr_t)p & 15u) || available < 20)
    return 0;
  static const uint8_t signature[8] = {'R','S','D',' ','P','T','R',' '};
  for (size_t i = 0; i < sizeof(signature); ++i)
    if (p[i] != signature[i]) return 0;
  if (p[15] != 0 && p[15] != 2) return 0;
  uint32_t size = p[15] ? 36u : 20u;
  if (available < size) return 0;
  if (size == 36 && (uint32_t)p[20] + ((uint32_t)p[21] << 8) +
                        ((uint32_t)p[22] << 16) + ((uint32_t)p[23] << 24) != 36)
    return 0;
  if (size == 36 && (p[33] || p[34] || p[35])) return 0;
  uint8_t sum = 0;
  for (uint32_t i = 0; i < 20; ++i) sum = (uint8_t)(sum + p[i]);
  if (sum) return 0;
  if (size == 36) {
    sum = 0;
    for (uint32_t i = 0; i < size; ++i) sum = (uint8_t)(sum + p[i]);
    if (sum) return 0;
  }
  *length = size;
  *revision = p[15];
  return 1;
}
#endif
