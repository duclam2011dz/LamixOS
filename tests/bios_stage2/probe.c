/* SPDX-License-Identifier: GPL-3.0-only */
#include "lamix/boot/contract.h"
#include <stdint.h>
#ifndef PROBE_FAULT
#define PROBE_FAULT 0
#endif
uint64_t probe_snapshot[9];
volatile uint64_t probe_data = UINT64_C(0xfedcba9876543210);
static volatile uint8_t zero_bss[8192];
static const char rodata[] = "LAMIX M05 ELF64";
static uint8_t in8(uint16_t p) {
  uint8_t x;
  __asm__ volatile("inb %1,%0" : "=a"(x) : "Nd"(p));
  return x;
}
static void out8(uint16_t p, uint8_t v) {
  __asm__ volatile("outb %0,%1" ::"a"(v), "Nd"(p));
}
static void serial(const char *s) {
  for (; *s; s++) {
    for (unsigned i = 65535; i && !(in8(0x3fd) & 32); i--) {
    }
    out8(0x3f8, (uint8_t)*s);
  }
}
__attribute__((noreturn)) static void fail(void) {
  serial("M05:FAIL\nENTRY\n");
  for (;;)
    __asm__ volatile("cli; hlt");
}
static uint64_t read64(const uint8_t *p) {
  uint64_t n = 0;
  for (unsigned i = 0; i < 8; i++)
    n |= (uint64_t)p[i] << (i * 8);
  return n;
}
static uint32_t read32(const uint8_t *p) { return (uint32_t)read64(p); }
static const uint8_t *entry_record;
static const uint64_t *physical_table(uint64_t physical) {
  if (!entry_record)
    fail();
  uint64_t count = read64(entry_record + 64);
  for (uint64_t i = 0; i < count; i++) {
    const uint8_t *s = entry_record + 72 + i * 32;
    uint64_t base = read64(s), length = read64(s + 16);
    if (physical >= base && physical - base <= length &&
        4096 <= length - (physical - base))
      return (const uint64_t *)(uintptr_t)(read64(s + 8) + (physical - base));
  }
  fail();
}
static uint64_t pte(uint64_t va) {
  const uint64_t *t = physical_table(probe_snapshot[5]);
  for (unsigned s = 39; s > 12; s -= 9) {
    uint64_t e = t[(va >> s) & 511];
    if (!(e & 1) || e & 128)
      return 0;
    t = physical_table(e & UINT64_C(0x000ffffffffff000));
  }
  return t[(va >> 12) & 511];
}
static uint32_t leaf_count(uint64_t base, unsigned level) {
  const uint64_t *t = physical_table(base);
  uint32_t count = 0;
  for (unsigned i = 0; i < 512; i++)
    if (t[i] & 1) {
      if (level == 1)
        count++;
      else {
        if (t[i] & 128)
          fail();
        count += leaf_count(t[i] & UINT64_C(0x000ffffffffff000), level - 1);
      }
    }
  return count;
}
void probe_main(void) {
  const uint8_t *blob = (void *)(uintptr_t)probe_snapshot[0];
  lbp_view view;
  if (probe_snapshot[1] != LBP_ENTRY_MAGIC || !blob ||
      lbp_validate(blob, LBP_MAX_SIZE, 0, 0, &view) != LBP_OK)
    fail();
  if ((probe_snapshot[4] & UINT64_C(0x80010001)) != UINT64_C(0x80010001) ||
      !(probe_snapshot[6] & 32) || (probe_snapshot[6] & (1u << 12)) ||
      (probe_snapshot[7] & 0x500) != 0x500 || (probe_snapshot[3] & 0x600) ||
      (probe_snapshot[8] & 3) || !((probe_snapshot[8] >> 16) & 0xffff) ||
      !((probe_snapshot[8] >> 32) & 0xffff))
    fail();
  int nx = !!(read64(blob + 32) & LBP_FEATURE_NX);
  if (nx != !!(probe_snapshot[7] & 0x800))
    fail();
  if (probe_data != UINT64_C(0xfedcba9876543210) || rodata[6] != 'M')
    fail();
  uint8_t gdtr[10];
  __asm__ volatile("sgdt %0" : "=m"(gdtr));
  uint64_t descriptor = read64((const uint8_t *)(uintptr_t)read64(gdtr + 2) +
                               (probe_snapshot[8] & 0xfff8));
  if ((descriptor & UINT64_C(0x0060980000000000)) !=
          UINT64_C(0x0020980000000000) ||
      !(probe_snapshot[8] >> 48))
    fail();
  for (unsigned i = 0; i < sizeof zero_bss; i++)
    if (zero_bss[i])
      fail();
  const uint8_t *entry = 0;
  for (uint32_t i = 0; i < view.count; i++) {
    uint64_t off;
    if (lbp_record_at(&view, i, &off) != LBP_OK)
      fail();
    if (read32(blob + off) == LBP_ENTRY)
      entry = blob + off;
  }
  if (!entry || read64(entry + 16) != probe_snapshot[5] ||
      read64(entry + 40) != probe_snapshot[2] || probe_snapshot[2] % 16 != 8 ||
      *(const uint64_t *)(uintptr_t)probe_snapshot[2])
    fail();
  entry_record = entry;
  if (pte(read64(entry + 48)) & 1)
    fail();
  uint64_t count = read64(entry + 64), pages = 0;
  for (uint64_t i = 0; i < count; i++) {
    const uint8_t *s = entry + 72 + i * 32;
    uint64_t base = read64(s), va = read64(s + 8), length = read64(s + 16);
    uint32_t perms = read32(s + 24);
    for (uint64_t off = 0; off < length; off += 4096) {
      uint64_t e = pte(va + off);
      pages++;
      if (!(e & 1) || (e & UINT64_C(0x000ffffffffff000)) != base + off ||
          !!(e & 2) != !!(perms & LBP_WRITE) ||
          (nx && !!(e >> 63) == !!(perms & LBP_EXECUTE)) || (!nx && (e >> 63)))
        fail();
    }
  }
  if (pages != leaf_count(probe_snapshot[5], 4))
    fail();
#if PROBE_FAULT == 1
  *(volatile uint8_t *)(uintptr_t)blob = 0;
  fail();
#elif PROBE_FAULT == 2
  (void)*(volatile uint8_t *)(uintptr_t)read64(entry + 48);
  fail();
#else
  serial("M05:PASS\n");
  for (;;)
    __asm__ volatile("cli; hlt");
#endif
}
