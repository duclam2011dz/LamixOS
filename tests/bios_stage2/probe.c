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
static void serial_hex(uint64_t value) {
  static const char digits[] = "0123456789abcdef";
  for (int shift = 60; shift >= 0; shift -= 4) {
    char c[2] = {digits[(value >> shift) & 15], 0};
    serial(c);
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
static uint16_t read16(const uint8_t *p) {
  return (uint16_t)p[0] | (uint16_t)((uint16_t)p[1] << 8);
}
static uint32_t read32(const uint8_t *p) { return (uint32_t)read64(p); }
static const uint8_t *entry_record;
static const uint8_t *reservation_record;
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
static uint64_t allocate_zero_table(void) {
  uint64_t count = read64(reservation_record + 24);
  uint32_t stride = read32(reservation_record + 20);
  for (uint64_t i = 0; i < count; i++) {
    const uint8_t *r = reservation_record + 32 + i * stride;
    if (read32(r + 16) != LBP_PURPOSE_TABLES)
      continue;
    uint64_t base = read64(r), length = read64(r + 8);
    for (uint64_t off = 0; off < length; off += 4096) {
      volatile const uint64_t *page = physical_table(base + off);
      if ((uintptr_t)page != base + off)
        continue;
      unsigned j = 0;
      for (; j < 512 && page[j] == 0; j++) {
      }
      if (j == 512)
        return base + off;
    }
  }
  fail();
}
static uint64_t *scratch_mapping(uint64_t va, uint64_t physical) {
  uint64_t *table = (uint64_t *)physical_table(probe_snapshot[5]);
  for (unsigned shift = 39; shift > 12; shift -= 9) {
    uint64_t *slot = &table[(va >> shift) & 511];
    if (!(*slot & 1))
      *slot = allocate_zero_table() | 3;
    table = (uint64_t *)(uintptr_t)(*slot & UINT64_C(0x000ffffffffff000));
  }
  uint64_t *slot = &table[(va >> 12) & 511];
  if (*slot & 1)
    fail();
  *slot = (physical & UINT64_C(0x000ffffffffff000)) | 1 |
          (probe_snapshot[7] & 0x800 ? UINT64_C(0x8000000000000000) : 0);
  return slot;
}
static lbp_status validate_with_acpi(const uint8_t *blob, uint64_t size,
                                     lbp_view *view) {
  uint64_t total = read64(blob + 16), at = read64(blob + 40);
  uint32_t records = read32(blob + 48);
  if (total > size || total > LBP_MAX_SIZE || records > LBP_MAX_RECORDS ||
      at < 64)
    return LBP_INVALID;
  const uint8_t *acpi = 0;
  uint64_t record_offsets[9] = {0};
  for (uint32_t i = 0; i < records; i++) {
    if (at > total || total - at < 16)
      return LBP_TRUNCATED;
    uint64_t length = read64(blob + at + 8);
    if (length < 16 || length % 8 || length > total - at)
      return LBP_INVALID;
    uint32_t type = read32(blob + at);
    if (type <= LBP_ACPI)
      record_offsets[type] = at;
    if (type == LBP_ACPI)
      acpi = blob + at;
    at += length;
  }
  if (at != total || !record_offsets[LBP_ENTRY] ||
      !record_offsets[LBP_RESERVATIONS])
    return LBP_INVALID;
  entry_record = blob + record_offsets[LBP_ENTRY];
  reservation_record = blob + record_offsets[LBP_RESERVATIONS];
  uint64_t entry_count = read64(entry_record + 64),
           res_count = read64(reservation_record + 24);
  uint32_t entry_stride = read32(entry_record + 60),
           res_stride = read32(reservation_record + 20);
  if (entry_stride < 32 || entry_stride % 8 || res_stride < 32 ||
      res_stride % 8 ||
      entry_count > (read64(entry_record + 8) - 72) / entry_stride ||
      res_count > (read64(reservation_record + 8) - 32) / res_stride)
    return LBP_INVALID;
  if (!acpi)
    return lbp_validate(blob, size, 0, 0, view);
  uint64_t physical = read64(acpi + 16);
  uint32_t length = read32(acpi + 24);
  if ((length != 20 && length != 36) || physical + length < physical)
    return LBP_INVALID;
  const uint64_t scratch = UINT64_C(0xfffffe0000000000);
  uint64_t first = physical & ~UINT64_C(4095),
           last = (physical + length - 1) & ~UINT64_C(4095);
  uint64_t *pte0 = scratch_mapping(scratch, first), *pte1 = 0;
  if (last != first)
    pte1 = scratch_mapping(scratch + 4096, last);
  const uint8_t *bytes =
      (const uint8_t *)(uintptr_t)(scratch + (physical - first));
  lbp_span span = {physical, bytes, length};
  lbp_status status = lbp_validate(blob, size, &span, 1, view);
  *pte0 = 0;
  if (pte1)
    *pte1 = 0;
  return status;
}
void probe_main(void) {
  const uint8_t *blob = (void *)(uintptr_t)probe_snapshot[0];
  lbp_view view;
  if (probe_snapshot[1] != LBP_ENTRY_MAGIC || !blob ||
      validate_with_acpi(blob, LBP_MAX_SIZE, &view) != LBP_OK)
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
  const uint8_t *device = 0;
  const uint8_t *fb = 0;
  const uint8_t *acpi = 0;
  for (uint32_t i = 0; i < view.count; i++) {
    uint64_t off;
    if (lbp_record_at(&view, i, &off) != LBP_OK)
      fail();
    if (read32(blob + off) == LBP_ENTRY)
      entry = blob + off;
    if (read32(blob + off) == LBP_BOOT_DEVICE)
      device = blob + off;
    if (read32(blob + off) == LBP_FRAMEBUFFER)
      fb = blob + off;
    if (read32(blob + off) == LBP_ACPI)
      acpi = blob + off;
    if (read32(blob + off) == LBP_RESERVATIONS)
      reservation_record = blob + off;
  }
  (void)acpi;
  if (!entry || !device || !reservation_record ||
      read64(entry + 16) != probe_snapshot[5] ||
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
  if (fb) {
    uint64_t first = read64(fb + 16) & ~UINT64_C(4095);
    uint64_t end = read64(fb + 16) + read64(fb + 24);
    if (end < read64(fb + 16))
      fail();
    for (uint64_t p = first; p < end; p += 4096) {
      uint64_t e = pte(p);
      if (!(e & 1) || (e & UINT64_C(0x000ffffffffff000)) != p || !(e & 2) ||
          (nx && !(e >> 63)) || (e & 0x18) != 0x18)
        fail();
    }
  }
  serial("M07:LBP ");
  serial_hex(read16(blob + 8));
  serial(" ");
  serial_hex(read16(blob + 10));
  serial(" ");
  serial_hex(read32(device + 16));
  serial(" ");
  serial_hex(read32(device + 72));
  serial(" ");
  serial_hex(read64(blob + 24));
  serial(" ");
  serial_hex(read64(blob + 32));
  serial(" ");
  serial_hex(read32(blob + 48));
  serial("\n");
#if PROBE_FAULT == 1
  *(volatile uint8_t *)(uintptr_t)blob = 0;
  fail();
#elif PROBE_FAULT == 2
  (void)*(volatile uint8_t *)(uintptr_t)read64(entry + 48);
  fail();
#else
  if (read32(device + 16) == LBP_SOURCE_UEFI) {
    if (fb)
      serial("M06:FRAMEBUFFER\n");
    if (acpi)
      serial("M06:ACPI\n");
    serial("M06:PASS\n");
  } else
    serial("M05:PASS\n");
  for (;;)
    __asm__ volatile("cli; hlt");
#endif
}
