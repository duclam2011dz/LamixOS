/* SPDX-License-Identifier: GPL-3.0-only */
#include "lamix/boot/loader.h"
#include "internal.h"

static int same(const void *a, const void *b, uint32_t n) {
  const uint8_t *x = a, *y = b;
  for (uint32_t i = 0; i < n; i++)
    if (x[i] != y[i])
      return 0;
  return 1;
}
static void copy(void *a, const void *b, uint32_t n) {
  uint8_t *x = a;
  const uint8_t *y = b;
  for (uint32_t i = 0; i < n; i++)
    x[i] = y[i];
}
static int empty(const void *p, uint32_t n) {
  const uint8_t *b = p;
  for (uint32_t i = 0; i < n; i++)
    if (b[i])
      return 0;
  return 1;
}
uint32_t lamix_crc32(const void *bytes, uint32_t size) {
  const uint8_t *p = bytes;
  uint32_t crc = ~0u;
  for (uint32_t i = 0; i < size; i++) {
    crc ^= p[i];
    for (unsigned b = 0; b < 8; b++)
      crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1)));
  }
  return ~crc;
}
static int read_bytes(lamix_reader r, uint64_t off, void *p, uint32_t n) {
  return r.read && off <= r.size && n <= r.size - off &&
         r.read(r.context, off, p, n);
}
static const uint8_t bios_guid[16] = {0x48, 0x61, 0x68, 0x21, 0x49, 0x64,
                                      0x6f, 0x6e, 0x74, 0x4e, 0x65, 0x65,
                                      0x64, 0x45, 0x46, 0x49};
static const uint8_t esp_guid[16] = {0x28, 0x73, 0x2a, 0xc1, 0x1f, 0xf8,
                                     0xd2, 0x11, 0xba, 0x4b, 0,    0xa0,
                                     0xc9, 0x3e, 0xc9, 0x3b};
static const uint8_t system_guid[16] = {0x0b, 0x41, 0xc2, 0xb2, 0x10, 0xb2,
                                        0xec, 0x56, 0xbd, 0xc3, 0x58, 0x90,
                                        0x72, 0xb4, 0xfe, 0x3d};

static int gpt_header(uint8_t *p, uint32_t current, uint32_t alternate,
                      uint32_t array, uint32_t sectors) {
  if (!same(p, "EFI PART", 8) || rd32(p + 8) != 0x10000 || rd32(p + 12) != 92 ||
      rd32(p + 20) || rd64(p + 24) != current || rd64(p + 32) != alternate ||
      rd64(p + 40) != 34 || rd64(p + 48) != (uint64_t)sectors - 34 ||
      rd64(p + 72) != array || rd32(p + 80) != 128 || rd32(p + 84) != 128 ||
      empty(p + 56, 16) || !empty(p + 92, 420))
    return 0;
  uint32_t expected = rd32(p + 16);
  uint8_t saved[4];
  copy(saved, p + 16, 4);
  for (unsigned i = 16; i < 20; i++)
    p[i] = 0;
  uint32_t actual = lamix_crc32(p, 92);
  copy(p + 16, saved, 4);
  return actual == expected;
}
static lbp_status gpt_open(lamix_reader r, uint32_t bs, uint32_t be,
                           uint32_t es, uint32_t ec, int uefi,
                           uint8_t *scratch, uint32_t capacity,
                           lamix_disk *out) {
  if (!scratch || capacity < 16384 || !out || !r.read || r.size % 512 ||
      r.size / 512 > UINT32_MAX || r.size / 512 < 68)
    return LBP_INVALID;
  uint32_t n = (uint32_t)(r.size / 512);
  uint8_t p[512], b[512];
  lamix_disk d = {0};
  d.reader = r;
  if (!read_bytes(r, 0, p, 512))
    return LBP_TRUNCATED;
  const uint8_t prefix[8] = {0, 0, 2, 0, 0xee, 0xff, 0xff, 0xff};
  if (!empty(p + 440, 6) || !same(p + 446, prefix, 8) || rd32(p + 454) != 1 ||
      rd32(p + 458) != n - 1 || !empty(p + 462, 48) || p[510] != 0x55 ||
      p[511] != 0xaa)
    return LBP_INVALID;
  if (!read_bytes(r, 512, p, 512) ||
      !read_bytes(r, (uint64_t)(n - 1) * 512, b, 512))
    return LBP_TRUNCATED;
  if (!gpt_header(p, 1, n - 1, 2, n) || !gpt_header(b, n - 1, 1, n - 33, n) ||
      !same(p + 56, b + 56, 16) || rd32(p + 88) != rd32(b + 88))
    return LBP_INVALID;
  if (!read_bytes(r, 1024, scratch, 16384))
    return LBP_TRUNCATED;
  if (lamix_crc32(scratch, 16384) != rd32(p + 88))
    return LBP_INVALID;
  for (uint32_t i = 0; i < 32; i++) {
    if (!read_bytes(r, (uint64_t)(n - 33 + i) * 512, b, 512))
      return LBP_TRUNCATED;
    if (!same(b, scratch + i * 512, 512))
      return LBP_CONFLICT;
  }
  copy(d.disk_guid, p + 56, 16);
  unsigned bios_count = 0, esp_count = 0, sys_count = 0;
  static const char *const names[5] = {"BIOS Boot", "ESP", "System A",
                                       "System B", "User Data"};
  static const uint32_t starts[5] = {2048, 6144, 530432, 4724736, 8919040};
  static const uint8_t user_guid[16] = {0x6f, 0xed, 0x1e, 0x88, 0xb9, 0xb4,
                                        0x1b, 0x5e, 0xb0, 0xbf, 0xc0, 0xa4,
                                        0xf1, 0xeb, 0x2d, 0xd9};
  unsigned used = 0;
  for (unsigned i = 0; i < 128; i++) {
    const uint8_t *e = scratch + i * 128;
    if (empty(e, 16)) {
      if (!empty(e, 128))
        return LBP_INVALID;
      continue;
    }
    if (i >= 5)
      return LBP_INVALID;
    used++;
    const uint8_t *expected = i == 0   ? bios_guid
                              : i == 1 ? esp_guid
                              : i < 4  ? system_guid
                                       : user_guid;
    if (!same(e, expected, 16))
      return LBP_INVALID;
    unsigned name_length = 0;
    for (; names[i][name_length]; name_length++)
      if (e[56 + 2 * name_length] != (uint8_t)names[i][name_length] ||
          e[57 + 2 * name_length])
        return LBP_INVALID;
    if (!empty(e + 56 + 2 * name_length, 72 - 2 * name_length))
      return LBP_INVALID;
    uint64_t first = rd64(e + 32), last = rd64(e + 40);
    uint64_t expected_end =
        i < 4 ? (uint64_t)starts[i + 1] - 1 : (uint64_t)n - 34;
    if (first != starts[i] || last != expected_end ||
        (i == 4 && last - first + 1 < 2048))
      return LBP_INVALID;
    if (first < 34 || last < first || last > n - 34 || first % 2048 ||
        rd64(e + 48) || empty(e + 16, 16) || same(e + 16, d.disk_guid, 16))
      return LBP_INVALID;
    for (unsigned j = 0; j < i; j++) {
      const uint8_t *q = scratch + j * 128;
      if (empty(q, 16))
        continue;
      if (same(q + 16, e + 16, 16) ||
          (first <= rd64(q + 40) && rd64(q + 32) <= last))
        return LBP_CONFLICT;
    }
    if (same(e, bios_guid, 16)) {
      bios_count++;
      d.bios_start = (uint32_t)first;
      d.bios_end = (uint32_t)last;
    }
    if (same(e, esp_guid, 16)) {
      esp_count++;
      d.esp_start = (uint32_t)first;
      d.esp_sectors = (uint32_t)(last - first + 1);
      copy(d.esp_guid, e + 16, 16);
    }
    if (same(e, system_guid, 16)) {
      static const uint8_t name_a[16] = {'S', 0, 'y', 0, 's', 0, 't', 0,
                                         'e', 0, 'm', 0, ' ', 0, 'A', 0};
      if (same(e + 56, name_a, 16) && empty(e + 72, 56)) {
        sys_count++;
        copy(d.system_guid, e + 16, 16);
      }
    }
  }
  if (used != 5 || bios_count != 1 || esp_count != 1 || sys_count != 1 ||
        (!uefi && (d.bios_start != bs || d.bios_end != be)) ||
        (uefi && (d.esp_start != es || d.esp_sectors != ec)))
    return LBP_CONFLICT;
  *out = d;
  return LBP_OK;
}
lbp_status lamix_gpt_open(lamix_reader r, uint32_t bs, uint32_t be,
                          uint8_t *scratch, uint32_t capacity,
                          lamix_disk *out) {
  return gpt_open(r, bs, be, 0, 0, 0, scratch, capacity, out);
}
lbp_status lamix_gpt_open_uefi(lamix_reader r, uint32_t es, uint32_t ec,
                               uint8_t *scratch, uint32_t capacity,
                               lamix_disk *out) {
  return gpt_open(r, 0, 0, es, ec, 1, scratch, capacity, out);
}

static int sector(const lamix_fat *f, uint32_t relative, uint8_t *p) {
  if (relative >= f->disk->esp_sectors)
    return 0;
  return read_bytes(f->disk->reader,
                    (uint64_t)(f->disk->esp_start + relative) * 512, p, 512);
}
/* FAT values: 0xffffffff error, >=0x0ffffff8 EOC. */
static uint32_t next_cluster(const lamix_fat *f, uint32_t c) {
  if (c < 2 || c >= f->clusters + 2)
    return UINT32_MAX;
  uint8_t p[512];
  uint32_t selected = 0;
  for (uint32_t i = 0; i < f->fat_count; i++) {
    if (!f->mirrored && i != f->active_fat)
      continue;
    uint32_t sec = f->fat_start + i * f->fat_sectors + c / 128;
    if (!sector(f, sec, p))
      return UINT32_MAX;
    uint32_t v = rd32(p + (c % 128) * 4) & 0x0fffffff;
    if ((f->mirrored && i && v != selected) || v < 2 || v == 0x0ffffff7 ||
        (v >= f->clusters + 2 && v < 0x0ffffff8))
      return UINT32_MAX;
    selected = v;
  }
  return selected;
}
static int cluster_sector(const lamix_fat *f, uint32_t c, uint32_t at,
                          uint8_t *p) {
  if (c < 2 || c >= f->clusters + 2 || at >= f->sectors_per_cluster)
    return 0;
  return sector(f, f->data_start + (c - 2) * f->sectors_per_cluster + at, p);
}
static int directory_chain(const lamix_fat *f, uint32_t c) {
  uint32_t slow = c, fast = c;
  for (uint32_t steps = 0; steps < f->clusters; steps++) {
    slow = next_cluster(f, slow);
    if (slow == UINT32_MAX)
      return 0;
    if (slow >= 0x0ffffff8)
      return 1;
    for (unsigned i = 0; i < 2 && fast < 0x0ffffff8; i++)
      fast = next_cluster(f, fast);
    if (fast == UINT32_MAX || (fast < 0x0ffffff8 && fast == slow))
      return 0;
  }
  return 0;
}
static int find_name(const lamix_fat *f, uint32_t dir, const char *name,
                     int directory, uint32_t *result, uint32_t *size) {
  uint32_t slow = dir, fast = dir, found = 0, answer = 0, length = 0;
  uint8_t p[512];
  if (!directory_chain(f, dir))
    return 0;
  for (uint32_t step = 0; step < f->clusters; step++) {
    for (uint32_t s = 0; s < f->sectors_per_cluster; s++) {
      if (!cluster_sector(f, dir, s, p))
        return 0;
      for (unsigned at = 0; at < 512; at += 32) {
        const uint8_t *e = p + at;
        if (e[0] == 0)
          goto done;
        if (e[0] == 0xe5 || e[11] == 0x0f || (e[11] & 8))
          continue;
        if (same(e, name, 11)) {
          if (found++ || !!(e[11] & 16) != directory || (e[11] & 0xc0))
            return 0;
          answer = ((uint32_t)rd16(e + 20) << 16) | rd16(e + 26);
          length = rd32(e + 28);
          if (answer < 2 || answer >= f->clusters + 2 || (directory && length))
            return 0;
        }
      }
    }
    dir = next_cluster(f, dir);
    if (dir == UINT32_MAX)
      return 0;
    if (dir >= 0x0ffffff8)
      goto done;
    slow = next_cluster(f, slow);
    for (unsigned k = 0; k < 2 && fast < 0x0ffffff8; k++)
      fast = next_cluster(f, fast);
    if (slow == UINT32_MAX || fast == UINT32_MAX ||
        (fast < 0x0ffffff8 && slow == fast))
      return 0;
  }
  return 0;
done:
  if (found != 1)
    return 0;
  *result = answer;
  *size = length;
  return 1;
}
lbp_status lamix_fat_open(const lamix_disk *d, lamix_fat *out) {
  if (!d || !out)
    return LBP_INVALID;
  lamix_fat f = {0};
  f.disk = d;
  uint8_t p[512];
  if (!sector(&f, 0, p))
    return LBP_TRUNCATED;
  uint32_t spc = p[13], reserved = rd16(p + 14), fats = p[16],
           fat = rd32(p + 36), total = rd32(p + 32);
  uint32_t flags = rd16(p + 40);
  if (rd16(p + 11) != 512 || !spc || spc > 64 || (spc & (spc - 1)) ||
      !reserved || fats < 1 || fats > 2 || rd16(p + 17) || rd16(p + 19) ||
      rd16(p + 22) || rd16(p + 42) || !fat || total > d->esp_sectors ||
      rd32(p + 28) != d->esp_start || p[510] != 0x55 || p[511] != 0xaa ||
      flags & 0xff70)
    return LBP_INVALID;
  uint64_t dat = (uint64_t)reserved + (uint64_t)fats * fat;
  if (dat >= total)
    return LBP_INVALID;
  uint32_t clusters = (uint32_t)((total - dat) / spc);
  if (clusters < 65525 || clusters > 0x0fffffed ||
      ((uint64_t)clusters + 2) * 4 > (uint64_t)fat * 512)
    return LBP_INVALID;
  f.fat_start = reserved;
  f.fat_sectors = fat;
  f.fat_count = fats;
  f.data_start = (uint32_t)dat;
  f.sectors_per_cluster = spc;
  f.clusters = clusters;
  f.root = rd32(p + 44);
  f.mirrored = !(flags & 0x80);
  f.active_fat = flags & 15;
  if ((!f.mirrored && f.active_fat >= fats) || f.root < 2 ||
      f.root >= clusters + 2)
    return LBP_INVALID;
  static const char names[4][12] = {"EFI        ", "LAMIX      ", "A          ",
                                    "KERNEL  ELF"};
  uint32_t dir = f.root, size = 0;
  for (unsigned i = 0; i < 4; i++)
    if (!find_name(&f, dir, names[i], i != 3, &dir, &size))
      return LBP_INVALID;
  if (!size || size > 16 * 1024 * 1024)
    return LBP_INVALID;
  f.file_cluster = dir;
  f.file_size = size;
  uint32_t count = (size - 1) / (spc * 512) + 1, c = dir;
  for (uint32_t i = 0; i < count; i++) {
    c = next_cluster(&f, c);
    if (c == UINT32_MAX || ((i + 1 == count) != (c >= 0x0ffffff8)))
      return LBP_INVALID;
  }
  *out = f;
  return LBP_OK;
}
int lamix_fat_read(void *ctx, uint64_t offset, void *bytes, uint32_t size) {
  lamix_fat *f = ctx;
  uint8_t *dest = bytes, p[512];
  if (!f || (!dest && size) || offset > f->file_size ||
      size > f->file_size - offset)
    return 0;
  uint32_t cluster_bytes = f->sectors_per_cluster * 512, c = f->file_cluster;
  uint32_t skip = (uint32_t)offset / cluster_bytes;
  for (uint32_t i = 0; i < skip; i++) {
    c = next_cluster(f, c);
    if (c >= 0x0ffffff8)
      return 0;
  }
  uint32_t within = (uint32_t)offset % cluster_bytes;
  while (size) {
    if (!cluster_sector(f, c, within / 512, p))
      return 0;
    uint32_t part = 512 - within % 512;
    if (part > size)
      part = size;
    copy(dest, p + within % 512, part);
    dest += part;
    size -= part;
    within += part;
    if (within == cluster_bytes && size) {
      within = 0;
      c = next_cluster(f, c);
      if (c >= 0x0ffffff8)
        return 0;
    }
  }
  return 1;
}

lbp_status lamix_elf_inspect(lamix_reader f, uint64_t features,
                             lamix_elf_plan *out) {
  uint8_t h[64], p[56];
  lamix_elf_plan plan = {0};
  uint32_t notes = 0;
  uint64_t total = 0;
  if (!out || f.size > 16 * 1024 * 1024 || !read_bytes(f, 0, h, 64))
    return LBP_TRUNCATED;
  const uint8_t ident[9] = {0x7f, 'E', 'L', 'F', 2, 1, 1, 0, 0};
  if (!same(h, ident, 9) || !empty(h + 9, 7) || rd16(h + 16) != 2 ||
      rd16(h + 18) != 62 || rd32(h + 20) != 1 || rd32(h + 48) ||
      rd16(h + 52) != 64 || rd16(h + 54) != 56 || !rd16(h + 56) ||
      rd16(h + 56) > 128)
    return LBP_INVALID;
  uint64_t phoff = rd64(h + 32);
  uint32_t count = rd16(h + 56);
  plan.entry = rd64(h + 24);
  if (phoff > f.size || (uint64_t)count * 56 > f.size - phoff)
    return LBP_TRUNCATED;
  for (uint32_t i = 0; i < count; i++) {
    if (!read_bytes(f, phoff + i * 56, p, 56))
      return LBP_TRUNCATED;
    uint32_t type = rd32(p), flags = rd32(p + 4);
    uint64_t off = rd64(p + 8), v = rd64(p + 16), phys = rd64(p + 24),
             sz = rd64(p + 32), mem = rd64(p + 40), align = rd64(p + 48);
    if (off > f.size || sz > f.size - off)
      return LBP_TRUNCATED;
    if (type == 1) {
      if (plan.count == 32 || !mem || sz > mem || phys ||
          v < UINT64_C(0xffffffff80000000) || v > UINT64_MAX - mem ||
          (v & 4095) || align < 4096 || (align & (align - 1)) ||
          v % align != off % align || flags & ~7u || !(flags & 4) ||
          (flags & 3) == 3 || mem > 64 * 1024 * 1024)
        return LBP_INVALID;
      uint64_t pages = (mem + 4095) & ~UINT64_C(4095);
      if (v > UINT64_MAX - pages || total > 64 * 1024 * 1024 - pages)
        return LBP_OVERFLOW;
      for (uint32_t j = 0; j < plan.count; j++) {
        uint64_t b = plan.segments[j].virtual_address,
                 n = (plan.segments[j].memory_size + 4095) & ~UINT64_C(4095);
        if (v < b + n && b < v + pages)
          return LBP_CONFLICT;
      }
      plan.segments[plan.count++] =
          (lamix_elf_segment){off,
                              v,
                              sz,
                              mem,
                              LBP_READ | ((flags & 2) ? LBP_WRITE : 0) |
                                  ((flags & 1) ? LBP_EXECUTE : 0),
                              0};
      total += pages;
    } else if (type == 4) {
      uint64_t at = off, end = off + sz;
      while (at < end) {
        uint8_t note[12], name[8], descriptor[32];
        if (end - at < 12 || !read_bytes(f, at, note, 12))
          return LBP_TRUNCATED;
        uint64_t ns = rd32(note), ds = rd32(note + 4),
                 nround = (ns + 3) & ~UINT64_C(3),
                 dround = (ds + 3) & ~UINT64_C(3);
        at += 12;
        if (nround > end - at || dround > end - at - nround)
          return LBP_TRUNCATED;
        if (ns == 6) {
          if (!read_bytes(f, at, name, 6))
            return LBP_TRUNCATED;
          if (same(name, "LAMIX\0", 6) && rd32(note + 8) == 1) {
            if (++notes != 1 || ds != 32)
              return LBP_CONFLICT;
            if (!read_bytes(f, at + nround, descriptor, 32))
              return LBP_TRUNCATED;
            lbp_status status = lbp_validate_note(descriptor, 32, features);
            if (status != LBP_OK)
              return status;
            plan.required_features = rd64(descriptor + 16);
          }
        }
        at += nround + dround;
      }
    } else if (type == 0 || type == 6) {
      /* Headers không trở thành một allocation độc lập. */
    } else if (type == 0x6474e551) {
      if (flags & 1)
        return LBP_INVALID;
    } else
      return LBP_INVALID;
  }
  if (notes != 1 || !plan.count)
    return LBP_INVALID;
  int executable = 0;
  for (uint32_t i = 0; i < plan.count; i++) {
    const lamix_elf_segment *s = plan.segments + i;
    if ((s->permissions & LBP_EXECUTE) && plan.entry >= s->virtual_address &&
        plan.entry - s->virtual_address < s->memory_size)
      executable = 1;
  }
  if (!executable)
    return LBP_INVALID;
  *out = plan;
  return LBP_OK;
}

/* Commit một validated plan; không publish segments khi lỗi. Các pages đã
 * cấp phát không tự free: caller giữ ownership và quyết định rollback/halt. */
lbp_status lamix_elf_load(lamix_reader file, const lamix_elf_plan *plan,
                          lamix_alloc_pages allocate, void *context,
                          lbp_segment *output, uint32_t capacity) {
  if (!plan || !allocate || !output || !plan->count || plan->count > 32)
    return LBP_INVALID;
  if (capacity < plan->count)
    return LBP_BUFFER_TOO_SMALL;
  lbp_segment segments[32];
  for (uint32_t i = 0; i < plan->count; i++) {
    const lamix_elf_segment *s = plan->segments + i;
    if (!s->memory_size || s->memory_size > 64 * 1024 * 1024 ||
        s->file_size > s->memory_size)
      return LBP_INVALID;
    uint32_t length = (uint32_t)((s->memory_size + 4095) & ~UINT64_C(4095));
    uint64_t physical = 0;
    void *mapped = 0;
    if (!allocate(context, length, &physical, &mapped))
      return LBP_BUFFER_TOO_SMALL;
    if (!mapped || !physical || physical % 4096 ||
        physical >= UINT64_C(0x100000000) ||
        length > UINT64_C(0x100000000) - physical)
      return LBP_INVALID;
    for (uint32_t j = 0; j < i; j++)
      if (physical < segments[j].physical + segments[j].length &&
          segments[j].physical < physical + length)
        return LBP_CONFLICT;
    uint8_t *bytes = mapped;
    for (uint32_t j = 0; j < length; j++)
      bytes[j] = 0;
    if (s->file_size &&
        !read_bytes(file, s->offset, mapped, (uint32_t)s->file_size))
      return LBP_TRUNCATED;
    segments[i] =
        (lbp_segment){physical, s->virtual_address, length, s->permissions, 0};
  }
  for (uint32_t i = 0; i < plan->count; i++)
    output[i] = segments[i];
  return LBP_OK;
}
