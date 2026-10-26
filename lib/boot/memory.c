/* SPDX-License-Identifier: GPL-3.0-only */
#include "internal.h"
#define LIMIT UINT64_C(4096)
#define PAGE UINT64_C(4096)
static uint64_t down(uint64_t x) { return x & ~(PAGE - 1); }
static uint64_t up(uint64_t x) { return (x + PAGE - 1) & ~(PAGE - 1); }
static uint32_t e820_type(uint32_t t) {
  switch (t) {
  case 1:
    return LBP_USABLE;
  case 3:
    return LBP_ACPI_RECLAIM;
  case 4:
    return LBP_ACPI_NVS;
  case 5:
    return LBP_BAD;
  default:
    return LBP_RESERVED;
  }
}
static uint32_t uefi_type(uint32_t t) {
  switch (t) {
  case 1:
  case 2:
  case 3:
  case 4:
    return LBP_LOADER_RECLAIM;
  case 7:
    return LBP_USABLE;
  case 5:
  case 6:
    return LBP_RUNTIME;
  case 8:
    return LBP_BAD;
  case 9:
    return LBP_ACPI_RECLAIM;
  case 10:
    return LBP_ACPI_NVS;
  default:
    return LBP_RESERVED;
  }
}
static lbp_status decode(const void *bytes, uint64_t size, uint32_t stride,
                         uint32_t version, int uefi, lbp_memory *out,
                         uint64_t cap, uint64_t *needed) {
  const uint8_t *p = bytes;
  uint64_t n = 0;
  if (!needed || (!p && size) || (!out && cap))
    return LBP_INVALID;
  if ((uefi && (version != 1 || stride < 40 || stride % 8)) ||
      (!uefi && stride != 20 && stride != 24))
    return LBP_UNSUPPORTED;
  if (size % stride)
    return LBP_TRUNCATED;
  if (size / stride > LIMIT)
    return LBP_INVALID;
  /* Hai lượt: lỗi input/capacity không ghi output một phần. */
  for (int pass = 0; pass < 2; pass++) {
    n = 0;
    for (uint64_t off = 0; off < size; off += stride) {
      const uint8_t *q = p + off;
      uint64_t base = rd64(q + (uefi ? 8 : 0)), len = rd64(q + (uefi ? 24 : 8));
      uint64_t attrs = uefi ? rd64(q + 32) : (stride == 24 ? rd32(q + 20) : 1);
      if (!uefi && stride == 24 && !(attrs & 1))
        continue;
      if (uefi) {
        if (len > UINT64_MAX / PAGE)
          return LBP_OVERFLOW;
        len *= PAGE;
      }
      if (!len)
        continue;
      if (!range(base, len))
        return LBP_OVERFLOW;
      uint32_t type = uefi ? uefi_type(rd32(q)) : e820_type(rd32(q + 16));
      uint32_t flags = uefi ? LBP_MEMORY_UEFI : 0;
      if (uefi && ((base % PAGE) != 0 || rd32(q + 4) != 0))
        return LBP_INVALID;
      if ((uefi && (attrs & UINT64_C(0x8000000000000000))) && type != LBP_BAD)
        type = LBP_RUNTIME;
      if ((uefi && (attrs & UINT64_C(0x8000))) || (!uefi && (attrs & 2))) {
        flags |= LBP_MEMORY_NONVOLATILE;
        if (type == LBP_USABLE || type == LBP_LOADER_RECLAIM)
          type = LBP_RESERVED;
      }
      if (pass)
        out[n] = (lbp_memory){base, len, type, flags, attrs};
      n++;
    }
    *needed = n;
    if (n > cap)
      return LBP_BUFFER_TOO_SMALL;
  }
  return LBP_OK;
}
lbp_status lbp_decode_e820(const void *b, uint64_t s, uint32_t stride,
                           lbp_memory *o, uint64_t c, uint64_t *n) {
  return decode(b, s, stride, 1, 0, o, c, n);
}
lbp_status lbp_decode_uefi(const void *b, uint64_t s, uint32_t stride,
                           uint32_t v, lbp_memory *o, uint64_t c, uint64_t *n) {
  return decode(b, s, stride, v, 1, o, c, n);
}
static uint32_t safe_type(const lbp_memory *m) {
  uint32_t t = m->type >= 1 && m->type <= 7 ? m->type : LBP_RESERVED;
  if ((m->flags & LBP_MEMORY_UEFI) &&
      (m->firmware_attributes & UINT64_C(0x8000000000000000)) && t != LBP_BAD)
    return LBP_RUNTIME;
  if (((m->flags & LBP_MEMORY_NONVOLATILE) ||
       (!(m->flags & LBP_MEMORY_UEFI) && (m->firmware_attributes & 2u)) ||
       ((m->flags & LBP_MEMORY_UEFI) &&
        (m->firmware_attributes & UINT64_C(0x8000)))) &&
      (t == LBP_USABLE || t == LBP_LOADER_RECLAIM))
    return LBP_RESERVED;
  return t;
}
static int reclaimable(uint32_t t) {
  return t == LBP_USABLE || t == LBP_LOADER_RECLAIM;
}
static unsigned priority(uint32_t t) {
  switch (t) {
  case LBP_BAD:
    return 7;
  case LBP_RUNTIME:
    return 6;
  case LBP_RESERVED:
    return 5;
  case LBP_ACPI_NVS:
    return 4;
  case LBP_ACPI_RECLAIM:
    return 3;
  case LBP_LOADER_RECLAIM:
    return 2;
  case LBP_USABLE:
    return 1;
  default:
    return 5;
  }
}
lbp_status lbp_normalize(const lbp_memory *in, uint64_t n,
                         const lbp_reservation *rs, uint64_t nr,
                         lbp_memory *tmp, uint64_t tc, lbp_memory *out,
                         uint64_t cap, uint64_t *needed) {
  uint64_t first = UINT64_MAX, last = 0, used = 0;
  if (!needed || (!in && n) || (!rs && nr) || (!out && cap) || n > LIMIT ||
      nr > LIMIT)
    return LBP_INVALID;
  *needed = 0;
  for (uint64_t i = 0; i < n; i++) {
    if (!in[i].length)
      continue;
    if (!range(in[i].base, in[i].length) ||
        in[i].base + in[i].length > UINT64_MAX - (PAGE - 1))
      return LBP_OVERFLOW;
    if (in[i].flags & ~3u)
      return LBP_INVALID;
    if (down(in[i].base) < first)
      first = down(in[i].base);
    if (up(in[i].base + in[i].length) > last)
      last = up(in[i].base + in[i].length);
  }
  for (uint64_t i = 0; i < nr; i++) {
    const lbp_reservation *r = rs + i;
    if (!range(r->base, r->length) ||
        r->base + r->length > UINT64_MAX - (PAGE - 1))
      return LBP_OVERFLOW;
    if (r->base % PAGE || r->length % PAGE || r->reserved || r->purpose < 1 ||
        r->purpose > 6 ||
        r->release != (r->purpose <= 2   ? 0u
                       : r->purpose == 3 ? 1u
                                         : 2u))
      return LBP_INVALID;
    for (uint64_t j = 0; j < i; j++)
      if (r->base < rs[j].base + rs[j].length &&
          rs[j].base < r->base + r->length)
        return LBP_CONFLICT;
    if (r->base < first || r->base + r->length > last)
      return LBP_CONFLICT;
  }
  if (!n && !nr)
    return LBP_OK;
  if (!tmp || tc < 4 * (n + nr) + 2) {
    *needed = 4 * (n + nr) + 2;
    return LBP_BUFFER_TOO_SMALL;
  }
  for (uint64_t pos = first; pos < last;) {
    uint64_t next = last;
    for (uint64_t i = 0; i < n + nr; i++) {
      uint64_t b = i < n ? in[i].base : rs[i - n].base,
               l = i < n ? in[i].length : rs[i - n].length;
      if (!l)
        continue;
      uint64_t points[4] = {down(b), up(b), down(b + l), up(b + l)};
      for (unsigned j = 0; j < 4; j++)
        if (points[j] > pos && points[j] < next)
          next = points[j];
    }
    lbp_memory value = {pos, next - pos, LBP_RESERVED, 0, 0};
    int found = 0, conflict = 0;
    for (uint64_t i = 0; i < n; i++) {
      const lbp_memory *m = in + i;
      if (!m->length || m->base >= next || m->base + m->length <= pos)
        continue;
      uint32_t t = safe_type(m);
      if ((m->base > pos || m->base + m->length < next) &&
          priority(t) < priority(LBP_RESERVED))
        t = LBP_RESERVED;
      if (found && (value.flags != m->flags ||
                    value.firmware_attributes != m->firmware_attributes))
        conflict = 1;
      if (!found || priority(t) > priority(value.type))
        value.type = t;
      if (!found) {
        value.flags = m->flags;
        value.firmware_attributes = m->firmware_attributes;
      }
      found = 1;
    }
    if (conflict) {
      if (priority(value.type) < priority(LBP_RESERVED))
        value.type = LBP_RESERVED;
      value.flags = 0;
      value.firmware_attributes = 0;
    }
    for (uint64_t i = 0; i < nr; i++)
      if (rs[i].base < next && rs[i].base + rs[i].length > pos) {
        if (!found || !reclaimable(value.type))
          return LBP_CONFLICT;
        value.type = LBP_RESERVED;
        found = 1;
      }
    if (found) {
      if (used && tmp[used - 1].base + tmp[used - 1].length == pos &&
          tmp[used - 1].type == value.type &&
          tmp[used - 1].flags == value.flags &&
          tmp[used - 1].firmware_attributes == value.firmware_attributes)
        tmp[used - 1].length += value.length;
      else
        tmp[used++] = value;
    }
    pos = next;
  }
  *needed = used;
  if (used > cap)
    return LBP_BUFFER_TOO_SMALL;
  for (uint64_t i = 0; i < used; i++)
    out[i] = tmp[i];
  return LBP_OK;
}
