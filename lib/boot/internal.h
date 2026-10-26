/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef LAMIX_BOOT_INTERNAL_H
#define LAMIX_BOOT_INTERNAL_H
#include <lamix/boot/contract.h>
static inline uint16_t rd16(const uint8_t *p) {
  return (uint16_t)((uint16_t)p[0] | (uint16_t)((uint16_t)p[1] << 8));
}
static inline uint32_t rd32(const uint8_t *p) {
  return (uint32_t)rd16(p) | ((uint32_t)rd16(p + 2) << 16);
}
static inline uint64_t rd64(const uint8_t *p) {
  return (uint64_t)rd32(p) | ((uint64_t)rd32(p + 4) << 32);
}
static inline int fits(uint64_t off, uint64_t len, uint64_t total) {
  return off <= total && len <= total - off;
}
static inline int range(uint64_t base, uint64_t len) {
  return len && len <= UINT64_MAX - base;
}
static inline int equal(const uint8_t *a, const char *b, uint64_t n) {
  for (uint64_t i = 0; i < n; i++)
    if (a[i] != (uint8_t)b[i])
      return 0;
  return 1;
}
static inline int zero(const uint8_t *p, uint64_t n) {
  for (uint64_t i = 0; i < n; i++)
    if (p[i])
      return 0;
  return 1;
}
static inline int canonical(uint64_t x) {
  return x <= UINT64_C(0x7fffffffffff) || x >= UINT64_C(0xffff800000000000);
}
static inline int low_range(uint64_t b, uint64_t n) {
  return b && range(b, n) && b + n <= UINT64_C(0x100000000);
}
#endif
