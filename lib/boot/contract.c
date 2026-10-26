/* SPDX-License-Identifier: GPL-3.0-only */
#include "internal.h"
#define PAGE UINT64_C(4096)
static int mask(uint32_t m) {
  if (!m)
    return 0;
  while (!(m & 1u))
    m >>= 1;
  return (m & (m + 1u)) == 0;
}
static int permissions(uint32_t p) {
  return (p & LBP_READ) && !(p & ~7u) && (p & 6u) != 6u;
}
static int pages(uint64_t b, uint64_t n) {
  return low_range(b, n) && !(b % PAGE) && !(n % PAGE);
}
static int vrange(uint64_t b, uint64_t n) {
  return range(b, n) && canonical(b) && canonical(b + n - 1) &&
         ((b >> 47) == ((b + n - 1) >> 47));
}
static int contains(uint64_t b, uint64_t n, uint64_t x, uint64_t l) {
  return range(b, n) && x >= b && fits(x - b, l, n);
}
lbp_status lbp_validate_note(const void *bytes, uint64_t size,
                             uint64_t features) {
  const uint8_t *p = bytes;
  if (!p)
    return LBP_INVALID;
  if (size < 32)
    return LBP_TRUNCATED;
  if (size != 32 || !equal(p, LBP_NOTE_MAGIC, 8) || rd32(p + 12))
    return LBP_INVALID;
  if (rd16(p + 8) != 1 || rd16(p + 10) > LBP_MINOR ||
      (rd64(p + 16) & ~LBP_FEATURES) || (rd64(p + 16) & ~features))
    return LBP_UNSUPPORTED;
  if (rd64(p + 16) & rd64(p + 24))
    return LBP_INVALID;
  return LBP_OK;
}
lbp_status lbp_record_at(const lbp_view *v, uint32_t index, uint64_t *offset) {
  if (!v || !v->bytes || !offset || index >= v->count || v->size < 64)
    return LBP_INVALID;
  uint64_t at = rd64(v->bytes + 40);
  for (uint32_t i = 0; i <= index; i++) {
    if (!fits(at, 16, v->size))
      return LBP_TRUNCATED;
    uint64_t len = rd64(v->bytes + at + 8);
    if (len < 16 || len % 8 || !fits(at, len, v->size))
      return LBP_INVALID;
    if (i == index) {
      *offset = at;
      return LBP_OK;
    }
    at += len;
  }
  return LBP_INVALID;
}
static lbp_status table(const uint8_t *p, uint64_t len, uint64_t prefix,
                        uint32_t min_stride, uint64_t *count,
                        uint32_t *stride) {
  if (len < prefix)
    return LBP_TRUNCATED;
  if (rd32(p + prefix - 16) != 1)
    return LBP_UNSUPPORTED;
  *stride = rd32(p + prefix - 12);
  *count = rd64(p + prefix - 8);
  if (*stride < min_stride || *stride % 8)
    return LBP_INVALID;
  if (*count > (len - prefix) / *stride)
    return LBP_TRUNCATED;
  if (*count > 4096)
    return LBP_INVALID;
  return LBP_OK;
}
static int covered_reserved(const uint8_t *map, uint64_t base, uint64_t len) {
  uint64_t pos = base, end = base + len, n = rd64(map + 24);
  uint32_t stride = rd32(map + 20);
  for (uint64_t i = 0; i < n && pos < end; i++) {
    const uint8_t *p = map + 32 + i * stride;
    uint64_t b = rd64(p), l = rd64(p + 8);
    if (b <= pos && b + l > pos && rd32(p + 16) == LBP_RESERVED)
      pos = b + l;
  }
  return pos >= end;
}
static int reservation(const uint8_t *rs, uint64_t b, uint64_t len,
                       uint32_t purpose) {
  uint64_t n = rd64(rs + 24);
  uint32_t stride = rd32(rs + 20);
  for (uint64_t i = 0; i < n; i++) {
    const uint8_t *p = rs + 32 + i * stride;
    if (rd32(p + 16) == purpose && contains(rd64(p), rd64(p + 8), b, len))
      return 1;
  }
  return 0;
}
static int mapped(const uint8_t *e, uint64_t phys, uint64_t virt, uint64_t len,
                  uint32_t perms) {
  uint64_t n = rd64(e + 64);
  uint32_t stride = rd32(e + 60);
  for (uint64_t i = 0; i < n; i++) {
    const uint8_t *p = e + 72 + i * stride;
    uint64_t b = rd64(p), v = rd64(p + 8), l = rd64(p + 16);
    if (contains(v, l, virt, len) && phys == b + (virt - v) &&
        rd32(p + 24) == perms)
      return 1;
  }
  return 0;
}
static int utf8(const uint8_t *p, uint64_t len) {
  for (uint64_t i = 0; i < len;) {
    uint32_t c = p[i++], min = 0;
    unsigned extra = 0;
    if (c < 0x80) {
      if (!c || c < 0x20 || c == 0x7f)
        return 0;
      continue;
    }
    if (c >= 0xc2 && c <= 0xdf) {
      c &= 31;
      extra = 1;
      min = 0x80;
    } else if (c >= 0xe0 && c <= 0xef) {
      c &= 15;
      extra = 2;
      min = 0x800;
    } else if (c >= 0xf0 && c <= 0xf4) {
      c &= 7;
      extra = 3;
      min = 0x10000;
    } else
      return 0;
    if (extra > len - i)
      return 0;
    while (extra--) {
      uint32_t d = p[i++];
      if ((d & 0xc0) != 0x80)
        return 0;
      c = (c << 6) | (d & 63);
    }
    if (c < min || c > 0x10ffff || (c >= 0xd800 && c <= 0xdfff))
      return 0;
  }
  return 1;
}
static const uint8_t *resolve(const lbp_span *sp, uint64_t n, uint64_t b,
                              uint64_t len) {
  for (uint64_t i = 0; i < n; i++)
    if (sp[i].bytes && contains(sp[i].physical, sp[i].size, b, len))
      return sp[i].bytes + (b - sp[i].physical);
  return NULL;
}
lbp_status lbp_validate(const void *bytes, uint64_t size, const lbp_span *sp,
                        uint64_t ns, lbp_view *view) {
  const uint8_t *p = bytes, *records[9] = {0};
  uint64_t offsets[9] = {0};
  if (!p || !view || (!sp && ns) || ns > 4096)
    return LBP_INVALID;
  for (uint64_t i = 0; i < ns; i++) {
    if (!sp[i].bytes || !range(sp[i].physical, sp[i].size))
      return LBP_INVALID;
    for (uint64_t j = 0; j < i; j++)
      if (sp[i].physical < sp[j].physical + sp[j].size &&
          sp[j].physical < sp[i].physical + sp[i].size)
        return LBP_CONFLICT;
  }
  if (size < 64)
    return LBP_TRUNCATED;
  if (!equal(p, LBP_MAGIC, 8))
    return LBP_INVALID;
  if (rd16(p + 8) != 1)
    return LBP_UNSUPPORTED;
  int future = rd16(p + 10) > LBP_MINOR;
  uint32_t hs = rd32(p + 12), nr = rd32(p + 48);
  uint64_t total = rd64(p + 16), at = rd64(p + 40), self = rd64(p + 56);
  if (hs < 64 || hs % 8 || total > LBP_MAX_SIZE || total % 8 || total < hs ||
      total > size || nr > LBP_MAX_RECORDS || rd32(p + 52) || at < hs ||
      at % 8 || at > total || self % 8 || !low_range(self, total))
    return LBP_INVALID;
  uint64_t req = rd64(p + 24), features = rd64(p + 32);
  if ((req & ~LBP_FEATURES) || (req & ~features))
    return LBP_UNSUPPORTED;
  if (!zero(p + hs, at - hs))
    return LBP_INVALID;
  for (uint32_t i = 0; i < nr; i++) {
    if (!fits(at, 16, total))
      return LBP_TRUNCATED;
    const uint8_t *r = p + at;
    uint32_t type = rd32(r), flags = rd32(r + 4);
    uint64_t len = rd64(r + 8);
    if (len < 16 || len % 8 || !fits(at, len, total) || flags & ~LBP_REQUIRED)
      return LBP_INVALID;
    if (type >= 1 && type <= 8) {
      if (records[type])
        return LBP_CONFLICT;
      records[type] = r;
      offsets[type] = at;
    } else if (flags & LBP_REQUIRED)
      return LBP_UNSUPPORTED;
    at += len;
  }
  if (at != total)
    return LBP_INVALID;
  for (unsigned i = 1; i <= 5; i++)
    if (!records[i] || !(rd32(records[i] + 4) & LBP_REQUIRED))
      return LBP_INVALID;
  for (unsigned i = 1; i <= 8; i++)
    if (records[i] && (i <= 4 || i == 6)) {
      uint64_t n;
      uint32_t stride;
      uint64_t pre = i == 2 ? 40 : i == 4 ? 72 : 32;
      lbp_status status =
          table(records[i], rd64(records[i] + 8), pre, 32, &n, &stride);
      if (status != LBP_OK)
        return status;
      if (i != 6 && !n)
        return LBP_INVALID;
      uint64_t used = pre + n * stride;
      if (!future && i != 6 &&
          !zero(records[i] + used, rd64(records[i] + 8) - used))
        return LBP_INVALID;
      for (uint64_t j = 0; j < n; j++)
        if (!future && !zero(records[i] + pre + j * stride + 32, stride - 32))
          return LBP_INVALID;
    }
  const uint8_t *m = records[1], *k = records[2], *rs = records[3],
                *e = records[4], *d = records[5];
  uint64_t count = rd64(m + 24), end = 0;
  for (uint64_t i = 0; i < count; i++) {
    const uint8_t *q = m + 32 + i * rd32(m + 20);
    uint64_t b = rd64(q), l = rd64(q + 8);
    uint32_t t = rd32(q + 16), f = rd32(q + 20);
    if (!range(b, l) || b % PAGE || l % PAGE || b < end || t < 1 || t > 7 ||
        f & ~3u || ((f & 2u) && (t == 1 || t == 6 || t == 4)))
      return LBP_INVALID;
    if ((f & LBP_MEMORY_UEFI) &&
        (rd64(q + 24) & UINT64_C(0x8000000000000000)) && t != LBP_RUNTIME &&
        t != LBP_BAD)
      return LBP_INVALID;
    if ((f & LBP_MEMORY_UEFI) && (rd64(q + 24) & UINT64_C(0x8000)) &&
        (t == LBP_USABLE || t == LBP_LOADER_RECLAIM || t == LBP_ACPI_RECLAIM))
      return LBP_INVALID;
    if (!(f & LBP_MEMORY_UEFI) && (rd64(q + 24) & 2u) &&
        (t == LBP_USABLE || t == LBP_LOADER_RECLAIM || t == LBP_ACPI_RECLAIM))
      return LBP_INVALID;
    end = b + l;
  }
  count = rd64(rs + 24);
  for (uint64_t i = 0; i < count; i++) {
    const uint8_t *q = rs + 32 + i * rd32(rs + 20);
    uint64_t b = rd64(q), l = rd64(q + 8);
    uint32_t purpose = rd32(q + 16), release = rd32(q + 20);
    if (!pages(b, l) || rd64(q + 24) || purpose < 1 || purpose > 6 ||
        release != (purpose <= 2   ? 0u
                    : purpose == 3 ? 1u
                                   : 2u) ||
        !covered_reserved(m, b, l))
      return LBP_INVALID;
    for (uint64_t j = 0; j < i; j++) {
      const uint8_t *v = rs + 32 + j * rd32(rs + 20);
      if (b < rd64(v) + rd64(v + 8) && rd64(v) < b + l)
        return LBP_CONFLICT;
    }
  }
  uint64_t cr3 = rd64(e + 16), stack = rd64(e + 24), stacksize = rd64(e + 32),
           rsp = rd64(e + 40), guard = rd64(e + 48);
  if (!pages(cr3, PAGE) || !pages(stack, stacksize) || stacksize < 65536 ||
      rsp != stack + stacksize - 8 || rsp % 16 != 8 || guard + PAGE != stack ||
      guard % PAGE || !reservation(rs, cr3, PAGE, LBP_PURPOSE_TABLES) ||
      !reservation(rs, stack, stacksize, LBP_PURPOSE_STACK) ||
      !reservation(rs, self, total, LBP_PURPOSE_BOOTINFO))
    return LBP_INVALID;
  count = rd64(e + 64);
  for (uint64_t i = 0; i < count; i++) {
    const uint8_t *q = e + 72 + i * rd32(e + 60);
    uint64_t b = rd64(q), v = rd64(q + 8), l = rd64(q + 16);
    uint32_t perms = rd32(q + 24);
    if (!pages(b, l) || !vrange(v, l) || v % PAGE || !permissions(perms) ||
        rd32(q + 28) || (v < guard + PAGE && guard < v + l))
      return LBP_INVALID;
    for (uint64_t j = 0; j < i; j++) {
      const uint8_t *a = e + 72 + j * rd32(e + 60);
      uint64_t av = rd64(a + 8), al = rd64(a + 16), ab = rd64(a);
      if (v < av + al && av < v + l)
        return LBP_CONFLICT;
      if (b < ab + al && ab < b + l && ((perms | rd32(a + 24)) & 6u) == 6u)
        return LBP_CONFLICT;
    }
    if (b < self + total && self < b + l && perms != LBP_READ)
      return LBP_CONFLICT;
    /* Mapping RAM phải có owner; vùng reserved firmware không tự thành mapping.
     */
    int owner = 0;
    for (uint64_t j = 0; j < rd64(rs + 24); j++) {
      const uint8_t *a = rs + 32 + j * rd32(rs + 20);
      if (contains(rd64(a), rd64(a + 8), b, l))
        owner = 1;
    }
    if (!owner)
      return LBP_INVALID;
  }
  uint64_t bp = self & ~(PAGE - 1),
           bl = ((self + total + PAGE - 1) & ~(PAGE - 1)) - bp;
  if (!mapped(e, bp, bp, bl, LBP_READ) ||
      !mapped(e, stack, stack, stacksize, LBP_READ | LBP_WRITE))
    return LBP_INVALID;
  uint64_t entry = rd64(k + 16);
  int executable = 0;
  for (uint64_t i = 0; i < rd64(k + 32); i++) {
    const uint8_t *q = k + 40 + i * rd32(k + 28);
    uint64_t b = rd64(q), v = rd64(q + 8), l = rd64(q + 16);
    uint32_t perms = rd32(q + 24);
    if (!pages(b, l) || !vrange(v, l) || v < UINT64_C(0xffffffff80000000) ||
        v % PAGE || !permissions(perms) || rd32(q + 28) ||
        !reservation(rs, b, l, LBP_PURPOSE_KERNEL) ||
        !mapped(e, b, v, l, perms))
      return LBP_INVALID;
    if ((perms & LBP_EXECUTE) && contains(v, l, entry, 1))
      executable = 1;
    for (uint64_t j = 0; j < i; j++) {
      const uint8_t *a = k + 40 + j * rd32(k + 28);
      if ((b < rd64(a) + rd64(a + 16) && rd64(a) < b + l) ||
          (v < rd64(a + 8) + rd64(a + 16) && rd64(a + 8) < v + l))
        return LBP_CONFLICT;
    }
  }
  if (!executable)
    return LBP_INVALID;
  if (rd64(d + 8) < 80 || (!future && rd64(d + 8) != 80) || rd32(d + 76) ||
      (rd32(d + 16) != 1 && rd32(d + 16) != 2) || rd32(d + 20) != 512 ||
      (rd32(d + 72) != 1 && rd32(d + 72) != 2) || zero(d + 24, 16) ||
      zero(d + 40, 16) || zero(d + 56, 16))
    return LBP_INVALID;
  if (equal(d + 24, (const char *)(d + 40), 16) ||
      equal(d + 24, (const char *)(d + 56), 16) ||
      equal(d + 40, (const char *)(d + 56), 16))
    return LBP_CONFLICT;
  if (records[6]) {
    const uint8_t *r = records[6];
    uint64_t n = rd64(r + 24), data = offsets[6] + 32 + n * rd32(r + 20),
             limit = offsets[6] + rd64(r + 8);
    for (uint64_t i = 0; i < n; i++) {
      const uint8_t *q = r + 32 + i * rd32(r + 20);
      uint64_t b = rd64(q), l = rd64(q + 8), name = rd64(q + 16);
      uint32_t nl = rd32(q + 24);
      if (!low_range(b, l) || !reservation(rs, b, l, LBP_PURPOSE_MODULE) ||
          rd32(q + 28) || !nl || name < data || !fits(name, nl, limit) ||
          !utf8(p + name, nl))
        return LBP_INVALID;
      for (uint64_t j = 0; j < i; j++) {
        const uint8_t *a = r + 32 + j * rd32(r + 20);
        if (b < rd64(a) + rd64(a + 8) && rd64(a) < b + l)
          return LBP_CONFLICT;
      }
    }
    /* Tên lưu liên tục theo thứ tự descriptor, padding cuối zero. */
    for (uint64_t i = 0; i < n; i++) {
      const uint8_t *q = r + 32 + i * rd32(r + 20);
      if (rd64(q + 16) != data)
        return LBP_INVALID;
      data += rd32(q + 24);
    }
    if (!future && !zero(p + data, limit - data))
      return LBP_INVALID;
  }
  if (((features & 1) != 0) != (records[7] != NULL) ||
      ((features & 2) != 0) != (records[8] != NULL))
    return LBP_INVALID;
  if (records[7]) {
    const uint8_t *r = records[7];
    if (rd64(r + 8) < 64 || (!future && rd64(r + 8) != 64))
      return LBP_INVALID;
    uint64_t b = rd64(r + 16), l = rd64(r + 24);
    uint32_t w = rd32(r + 32), h = rd32(r + 36), pitch = rd32(r + 40),
             bpp = rd32(r + 44);
    uint32_t red = rd32(r + 48), green = rd32(r + 52), blue = rd32(r + 56),
             res = rd32(r + 60);
    if (!range(b, l) || !w || !h || bpp != 32 || pitch % 4 ||
        pitch < (uint64_t)w * 4 || (uint64_t)pitch * h > l || !mask(red) ||
        !mask(green) || !mask(blue) || (res && !mask(res)) || (red & green) ||
        (red & blue) || (green & blue) || (res & (red | green | blue)))
      return LBP_INVALID;
    for (uint64_t i = 0; i < rd64(rs + 24); i++) {
      const uint8_t *q = rs + 32 + i * rd32(rs + 20);
      if (b < rd64(q) + rd64(q + 8) && rd64(q) < b + l)
        return LBP_CONFLICT;
    }
    for (uint64_t i = 0; i < rd64(m + 24); i++) {
      const uint8_t *q = m + 32 + i * rd32(m + 20);
      if (b < rd64(q) + rd64(q + 8) && rd64(q) < b + l &&
          (rd32(q + 16) == LBP_USABLE || rd32(q + 16) == LBP_LOADER_RECLAIM))
        return LBP_CONFLICT;
    }
  }
  if (records[8]) {
    const uint8_t *r = records[8];
    if (rd64(r + 8) < 32 || (!future && rd64(r + 8) != 32))
      return LBP_INVALID;
    uint64_t b = rd64(r + 16);
    uint32_t l = rd32(r + 24), rev = rd32(r + 28);
    if (!range(b, l) || b % 16 || (rev != 0 && rev != 2) ||
        (rev == 0 ? l != 20 : l != 36))
      return LBP_INVALID;
    const uint8_t *a = resolve(sp, ns, b, l);
    if (!a)
      return LBP_TRUNCATED;
    if (!equal(a, "RSD PTR ", 8) || a[15] != rev)
      return LBP_INVALID;
    unsigned sum = 0;
    for (unsigned i = 0; i < 20; i++)
      sum += a[i];
    if (sum % 256)
      return LBP_INVALID;
    if (rev == 2) {
      if (rd32(a + 20) != 36 || !zero(a + 33, 3))
        return LBP_INVALID;
      sum = 0;
      for (unsigned i = 0; i < 36; i++)
        sum += a[i];
      if (sum % 256)
        return LBP_INVALID;
    }
    for (uint64_t i = 0; i < rd64(rs + 24); i++) {
      const uint8_t *q = rs + 32 + i * rd32(rs + 20);
      if (b < rd64(q) + rd64(q + 8) && rd64(q) < b + l)
        return LBP_CONFLICT;
    }
    /* RSDP không nằm trong vùng được cấp phát ngay. */
    for (uint64_t i = 0; i < rd64(m + 24); i++) {
      const uint8_t *q = m + 32 + i * rd32(m + 20);
      if (b < rd64(q) + rd64(q + 8) && rd64(q) < b + l &&
          (rd32(q + 16) == LBP_USABLE || rd32(q + 16) == LBP_LOADER_RECLAIM))
        return LBP_CONFLICT;
    }
  }
  *view = (lbp_view){p, total, nr};
  return LBP_OK;
}
