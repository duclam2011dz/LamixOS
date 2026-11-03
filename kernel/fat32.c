/* SPDX-License-Identifier: GPL-3.0-only */
#include "fat32.h"

static int bytes_equal(const void *left, const void *right, size_t size) {
  const uint8_t *a = left, *b = right;
  for (size_t i = 0; i < size; ++i)
    if (a[i] != b[i])
      return 0;
  return 1;
}
static void bytes_copy(void *target, const void *source, size_t size) {
  uint8_t *d = target;
  const uint8_t *s = source;
  for (size_t i = 0; i < size; ++i)
    d[i] = s[i];
}

static uint16_t u16(const uint8_t *p) {
  return (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
}
static uint32_t u32(const uint8_t *p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
         ((uint32_t)p[3] << 24);
}
static void p16(uint8_t *p, uint16_t v) {
  p[0] = (uint8_t)v;
  p[1] = (uint8_t)(v >> 8);
}
static void p32(uint8_t *p, uint32_t v) {
  for (unsigned i = 0; i < 4; i++)
    p[i] = (uint8_t)(v >> (i * 8));
}
static void short_name(const uint8_t *e, char *out);
static vfs_status next_cluster(fat32 *f, uint32_t c, uint32_t *out);
static vfs_status find_entry(fat32 *f, uint32_t directory_cluster,
                             const char *name, uint32_t *sector,
                             uint16_t *slot, uint8_t *out);
static vfs_status validate_file_chain(fat32 *f, uint32_t first_cluster,
                                      uint64_t size);
static vfs_status free_chain(fat32 *f, uint32_t first_cluster);
static uint32_t lfn_checksum(const uint8_t *n);
typedef struct fat_dir_slot {
  uint32_t sector;
  uint16_t slot;
  uint8_t prior_first;
} fat_dir_slot;
static vfs_status read_sector(fat32 *f, uint32_t rel, uint8_t *out) {
  if (rel >= f->sectors)
    return VFS_CORRUPT;
  block_request r;
  block_request_init(&r, BLOCK_READ, f->start_lba + rel, 1, out, 0, 0);
  return block_io_sync(f->device, &r, 10000000) == BLOCK_OK ? VFS_OK
                                                            : VFS_IO_ERROR;
}
static vfs_status write_sector(fat32 *f, uint32_t rel, const uint8_t *in) {
  if (rel >= f->sectors || f->device->read_only)
    return VFS_READ_ONLY;
  block_request r;
  block_request_init(&r, BLOCK_WRITE, f->start_lba + rel, 1, (void *)in, 0, 0);
  return block_io_sync(f->device, &r, 10000000) == BLOCK_OK ? VFS_OK
                                                            : VFS_IO_ERROR;
}
static uint32_t fat_sector(fat32 *f, uint32_t c) {
  return f->reserved + f->active_fat * f->fat_sectors + (c * 4u) / 512u;
}
static vfs_status set_cluster(fat32 *f, uint32_t c, uint32_t value) {
  if (c < 2 || c >= f->clusters + 2 || value > 0x0fffffff)
    return VFS_CORRUPT;
  uint32_t rel = f->reserved + (c * 4u) / 512u;
  for (uint32_t copy = 0; copy < f->fat_count; ++copy) {
    if (!f->mirrored && copy != f->active_fat)
      continue;
    vfs_status s = read_sector(f, rel + copy * f->fat_sectors, f->sector);
    if (s != VFS_OK)
      return s;
    uint32_t at = (c * 4u) % 512u;
    p32(f->sector + at, (u32(f->sector + at) & 0xf0000000) | value);
    s = write_sector(f, rel + copy * f->fat_sectors, f->sector);
    if (s != VFS_OK)
      return s;
  }
  return VFS_OK;
}
static vfs_status allocate_cluster(fat32 *f, uint32_t *out) {
  for (uint32_t c = 2; c < f->clusters + 2; ++c) {
    uint32_t next;
    vfs_status s = next_cluster(f, c, &next);
    if (s != VFS_OK && s != VFS_CORRUPT)
      return s;
    /* Zero is a free FAT entry; EOC and malformed links are never reused. */
    s = read_sector(f, fat_sector(f, c), f->sector);
    if (s != VFS_OK)
      return s;
    if ((u32(f->sector + ((c * 4u) % 512)) & 0x0fffffff) != 0)
      continue;
    s = set_cluster(f, c, 0x0fffffff);
    if (s != VFS_OK)
      return s;
    uint8_t zero[512] = {0};
    for (uint32_t i = 0; i < f->spc; ++i) {
      s = write_sector(f, f->data_start + (c - 2) * f->spc + i, zero);
      if (s != VFS_OK) {
        (void)set_cluster(f, c, 0);
        return s;
      }
    }
    *out = c;
    return VFS_OK;
  }
  return VFS_NO_MEMORY;
}
static vfs_status entry_write(fat32 *f, uint32_t rel, uint16_t slot,
                              const uint8_t entry[32]) {
  vfs_status s = read_sector(f, rel, f->sector);
  if (s != VFS_OK)
    return s;
  bytes_copy(f->sector + slot * 32u, entry, 32);
  return write_sector(f, rel, f->sector);
}
static vfs_status find_free_entry(fat32 *f, uint32_t directory_cluster,
                                  uint32_t *rel, uint16_t *slot) {
  if (directory_cluster < 2 || directory_cluster >= f->clusters + 2)
    return VFS_CORRUPT;
  uint32_t cluster = directory_cluster;
  for (uint32_t walks = 0; walks < f->clusters; ++walks) {
    for (uint32_t s = 0; s < f->spc; ++s) {
      uint32_t r = f->data_start + (cluster - 2) * f->spc + s;
      vfs_status st = read_sector(f, r, f->sector);
      if (st != VFS_OK)
        return st;
      for (uint16_t i = 0; i < 16; ++i)
        if (!f->sector[i * 32] || f->sector[i * 32] == 0xe5) {
          *rel = r;
          *slot = i;
          return VFS_OK;
        }
    }
    uint32_t next;
    vfs_status st = next_cluster(f, cluster, &next);
    if (st != VFS_OK)
      return st;
    if (!next) {
      st = allocate_cluster(f, &next);
      if (st != VFS_OK)
        return st;
      st = set_cluster(f, cluster, next);
      if (st != VFS_OK)
        return st;
      *rel = f->data_start + (next - 2) * f->spc;
      *slot = 0;
      return VFS_OK;
    }
    cluster = next;
  }
  return VFS_CORRUPT;
}
static vfs_status find_free_run(fat32 *f, uint32_t directory_cluster,
                                size_t needed, fat_dir_slot *slots) {
  if (directory_cluster < 2 || directory_cluster >= f->clusters + 2 ||
      !needed || needed > 21 || !slots)
    return VFS_INVALID;
  uint32_t cluster = directory_cluster;
  size_t run = 0;
  int past_end = 0;
  for (uint32_t walked = 0; walked < f->clusters; ++walked) {
    for (uint32_t s = 0; s < f->spc; ++s) {
      uint32_t rel = f->data_start + (cluster - 2) * f->spc + s;
      vfs_status st = read_sector(f, rel, f->sector);
      if (st != VFS_OK)
        return st;
      for (uint16_t i = 0; i < 16; ++i) {
        uint8_t first = f->sector[i * 32];
        if (!first)
          past_end = 1;
        if (past_end || !first || first == 0xe5) {
          slots[run++] = (fat_dir_slot){rel, i, first};
          if (run == needed)
            return VFS_OK;
        } else {
          run = 0;
        }
      }
    }
    uint32_t next;
    vfs_status st = next_cluster(f, cluster, &next);
    if (st != VFS_OK)
      return st;
    if (!next) {
      st = allocate_cluster(f, &next);
      if (st != VFS_OK)
        return st;
      st = set_cluster(f, cluster, next);
      if (st != VFS_OK) {
        (void)set_cluster(f, next, 0);
        return st;
      }
    }
    cluster = next;
  }
  return VFS_CORRUPT;
}
static int make_short(const char *name, uint8_t out[11]) {
  size_t n = 0, dot = SIZE_MAX, len = 0;
  while (name[len]) {
    if (name[len] == '.') {
      if (dot != SIZE_MAX)
        return 0;
      dot = len;
    }
    if (name[len] == '/' || name[len] < 0x21 || name[len] > 0x7e)
      return 0;
    ++len;
    if (len > 255)
      return 0;
  }
  size_t base = dot == SIZE_MAX ? len : dot;
  size_t ext = dot == SIZE_MAX ? 0 : len - dot - 1;
  if (!base || base > 8 || ext > 3 || (dot != SIZE_MAX && !ext))
    return 0;
  for (size_t i = 0; i < 11; ++i)
    out[i] = ' ';
  for (size_t i = 0; i < base; ++i) {
    char c = name[i];
    if (c == ' ' || c == '+' || c == ',' || c == ';' || c == '=' || c == '[' ||
        c == ']' || c == '\\')
      return 0;
    if (c >= 'a' && c <= 'z')
      c = (char)(c - 32);
    out[i] = (uint8_t)c;
  }
  for (size_t i = 0; i < ext; ++i) {
    char c = name[dot + 1 + i];
    if (c == ' ' || c == '+' || c == ',' || c == ';' || c == '=' || c == '[' ||
        c == ']' || c == '\\')
      return 0;
    if (c >= 'a' && c <= 'z')
      c = (char)(c - 32);
    out[8 + i] = (uint8_t)c;
  }
  n = base + ext;
  return n != 0;
}
static int name_has_lfn(const char *name, const uint8_t shortn[11]) {
  char rendered[13];
  short_name(shortn, rendered);
  size_t i = 0;
  for (; rendered[i] && name[i]; ++i) {
    char a = rendered[i], b = name[i];
    if (a >= 'A' && a <= 'Z')
      a = (char)(a + 32);
    if (b >= 'A' && b <= 'Z')
      b = (char)(b + 32);
    if (a != b)
      return 1;
  }
  return rendered[i] != name[i];
}
static vfs_status unique_short_alias(fat32 *f, uint32_t directory_cluster,
                                     const char *name, uint8_t out[11]) {
  size_t length = 0, dot = SIZE_MAX;
  while (name[length]) {
    if (name[length] == '.') {
      if (dot != SIZE_MAX)
        return VFS_INVALID;
      dot = length;
    }
    if ((uint8_t)name[length] < 0x20 || (uint8_t)name[length] > 0x7e ||
        name[length] == '/' || name[length] == '\\' || name[length] == ':' ||
        name[length] == '*' || name[length] == '?' || name[length] == '"' ||
        name[length] == '<' || name[length] == '>' || name[length] == '|')
      return VFS_INVALID;
    if (++length > 255)
      return VFS_TOO_LARGE;
  }
  size_t base = dot == SIZE_MAX ? length : dot;
  size_t ext = dot == SIZE_MAX ? 0 : length - dot - 1;
  if (!base || (dot != SIZE_MAX && !ext))
    return VFS_INVALID;
  if (ext > 3)
    ext = 3;
  for (uint32_t suffix = 1; suffix < 1000000; ++suffix) {
    uint32_t value = suffix;
    unsigned digits = 0;
    do {
      ++digits;
      value /= 10;
    } while (value);
    if (digits >= 7)
      return VFS_TOO_LARGE;
    for (unsigned i = 0; i < 11; ++i)
      out[i] = ' ';
    size_t prefix = base < 7 - digits ? base : 7 - digits;
    for (size_t i = 0; i < prefix; ++i) {
      char c = name[i];
      if (c >= 'a' && c <= 'z')
        c -= 32;
      if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_'))
        c = '_';
      out[i] = (uint8_t)c;
    }
    out[prefix] = '~';
    uint32_t remaining = suffix;
    for (unsigned i = 0; i < digits; ++i) {
      out[prefix + digits - i] = (uint8_t)('0' + remaining % 10);
      remaining /= 10;
    }
    for (size_t i = 0; i < ext; ++i) {
      char c = name[dot + 1 + i];
      if (c >= 'a' && c <= 'z')
        c -= 32;
      if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_'))
        c = '_';
      out[8 + i] = (uint8_t)c;
    }
    char rendered[13];
    short_name(out, rendered);
    uint32_t sector;
    uint16_t slot;
    uint8_t existing[32];
    vfs_status s = find_entry(f, directory_cluster, rendered, &sector, &slot,
                              existing);
    if (s == VFS_NOT_FOUND)
      return VFS_OK;
    if (s != VFS_OK)
      return s;
  }
  return VFS_NO_MEMORY;
}
static vfs_status write_lfn(fat32 *f, uint32_t directory_cluster,
                            const char *name, uint32_t *rel, uint16_t *slot,
                            const uint8_t shortn[11]) {
  size_t len = 0;
  while (name[len])
    ++len;
  size_t count = (len + 12) / 13;
  if (!count || count > 20)
    return VFS_TOO_LARGE;
  fat_dir_slot locations[21];
  vfs_status s = find_free_run(f, directory_cluster, count + 1, locations);
  if (s != VFS_OK)
    return s;
  size_t written = 0;
  for (size_t i = 0; i < count + 1; ++i) {
    uint8_t e[32] = {0};
    if (i == count) {
      bytes_copy(e, shortn, 11);
      e[11] = 0x20;
      s = entry_write(f, locations[i].sector, locations[i].slot, e);
      if (s == VFS_OK) {
        *rel = locations[i].sector;
        *slot = locations[i].slot;
        return VFS_OK;
      }
      written = i + 1;
      goto rollback;
    }
    size_t ordinal = count - i;
    e[0] = (uint8_t)ordinal | (i == 0 ? 0x40 : 0);
    e[11] = 0x0f;
    e[13] = lfn_checksum(shortn);
    p16(e + 26, 0);
    static const uint8_t offsets[] = {1,  3,  5,  7,  9,  14, 16,
                                      18, 20, 22, 24, 28, 30};
    for (unsigned j = 0; j < 13; ++j) {
      size_t at = (ordinal - 1) * 13 + j;
      uint16_t ch = at < len ? (uint8_t)name[at] : (at == len ? 0 : 0xffff);
      p16(e + offsets[j], ch);
    }
    s = entry_write(f, locations[i].sector, locations[i].slot, e);
    if (s != VFS_OK) {
      written = i + 1;
      goto rollback;
    }
    written = i + 1;
  }
  return VFS_CORRUPT;
rollback:
  for (size_t i = 0; i < written; ++i) {
    uint8_t old[32] = {0};
    old[0] = locations[i].prior_first;
    (void)entry_write(f, locations[i].sector, locations[i].slot, old);
  }
  return s;
}
static vfs_status next_cluster(fat32 *f, uint32_t c, uint32_t *out) {
  if (c < 2 || c >= f->clusters + 2)
    return VFS_CORRUPT;
  uint32_t n = 0;
  for (uint32_t copy = 0; copy < f->fat_count; ++copy) {
    if (!f->mirrored && copy != f->active_fat)
      continue;
    uint32_t rel = f->reserved + copy * f->fat_sectors + (c * 4u) / 512u;
    vfs_status s = read_sector(f, rel, f->sector);
    if (s != VFS_OK)
      return s;
    uint32_t value = u32(f->sector + ((c * 4u) % 512)) & 0x0fffffff;
    if ((f->mirrored && copy && value != n) || value == 0x0ffffff7 ||
        (value >= f->clusters + 2 && value < 0x0ffffff8) ||
        (value && value < 2))
      return VFS_CORRUPT;
    n = value;
  }
  if (n >= 0x0ffffff8) {
    *out = 0;
    return VFS_OK;
  }
  if (n < 2 || n >= f->clusters + 2)
    return VFS_CORRUPT;
  *out = n;
  return VFS_OK;
}
/* Directory walk is bounded by the volume cluster count, so loops become
 * errors. */
static uint32_t lfn_checksum(const uint8_t *n) {
  uint8_t s = 0;
  for (unsigned i = 0; i < 11; i++)
    s = (uint8_t)(((s & 1) ? 0x80 : 0) + (s >> 1) + n[i]);
  return s;
}
static void short_name(const uint8_t *e, char *out) {
  size_t n = 0;
  for (unsigned i = 0; i < 8 && e[i] != ' '; i++)
    out[n++] = (char)e[i];
  if (e[8] != ' ') {
    out[n++] = '.';
    for (unsigned i = 8; i < 11 && e[i] != ' '; i++)
      out[n++] = (char)e[i];
  }
  out[n] = 0;
}
static int lfn_piece(const uint8_t *e, uint16_t *chars, size_t capacity,
                     uint8_t *sum) {
  unsigned ord = e[0] & 0x1f;
  if (!ord || ord > 20 || e[11] != 0x0f || e[12] != 0 || e[13] != *sum)
    return 0;
  if (e[0] & 0x40)
    *sum = e[13];
  static const uint8_t offsets[] = {1,  3,  5,  7,  9,  14, 16,
                                    18, 20, 22, 24, 28, 30};
  for (unsigned i = 0; i < 13; i++) {
    unsigned at = (ord - 1) * 13 + i;
    if (at >= capacity)
      return 0;
    chars[at] = u16(e + offsets[i]);
  }
  return 1;
}
static vfs_status find_entry(fat32 *f, uint32_t directory_cluster,
                             const char *name, uint32_t *sector,
                             uint16_t *slot, uint8_t *out) {
  if (directory_cluster < 2 || directory_cluster >= f->clusters + 2)
    return VFS_CORRUPT;
  uint32_t cluster = directory_cluster;
  uint16_t chars[260];
  for (size_t i = 0; i < 260; i++)
    chars[i] = 0xffff;
  uint8_t sum = 0;
  int has_lfn = 0;
  for (uint32_t walked = 0; walked < f->clusters; walked++) {
    for (uint32_t s = 0; s < f->spc; s++) {
      uint32_t rel = f->data_start + (cluster - 2) * f->spc + s;
      vfs_status st = read_sector(f, rel, f->sector);
      if (st != VFS_OK)
        return st;
      for (uint16_t i = 0; i < 16; i++) {
        uint8_t *e = f->sector + i * 32;
        if (!e[0])
          return VFS_NOT_FOUND;
        if (e[0] == 0xe5) {
          has_lfn = 0;
          continue;
        }
        if (e[11] == 0x0f) {
          if (!has_lfn)
            sum = e[13];
          if (!lfn_piece(e, chars, 260, &sum))
            has_lfn = 0;
          else
            has_lfn = 1;
          continue;
        }
        if (e[11] & 0x08) {
          has_lfn = 0;
          continue;
        }
        if (has_lfn && sum != lfn_checksum(e))
          has_lfn = 0;
        char shortbuf[13];
        short_name(e, shortbuf);
        int match = 0;
        if (has_lfn) {
          size_t j = 0;
          match = 1;
          for (; j < 260 && chars[j] != 0 && chars[j] != 0xffff; j++) {
            char c = (char)chars[j];
            if (c >= 'A' && c <= 'Z')
              c = (char)(c + 32);
            char q = name[j];
            if (q >= 'A' && q <= 'Z')
              q = (char)(q + 32);
            if (c != q) {
              match = 0;
              break;
            }
          }
          if (name[j])
            match = 0;
        }
        if (!match) {
          size_t j = 0;
          match = 1;
          for (; shortbuf[j] && name[j]; j++) {
            char a = shortbuf[j], b = name[j];
            if (a >= 'A' && a <= 'Z')
              a = (char)(a + 32);
            if (b >= 'A' && b <= 'Z')
              b = (char)(b + 32);
            if (a != b) {
              match = 0;
              break;
            }
          }
          if (shortbuf[j] || name[j])
            match = 0;
        }
        if (match) {
          *sector = rel;
          *slot = i;
          bytes_copy(out, e, 32);
          return VFS_OK;
        }
        has_lfn = 0;
      }
    }
    uint32_t next;
    vfs_status st = next_cluster(f, cluster, &next);
    if (st != VFS_OK)
      return st;
    if (!next)
      return VFS_NOT_FOUND;
    cluster = next;
  }
  return VFS_CORRUPT;
}
typedef struct lfn_location {
  uint32_t sector;
  uint16_t slot;
} lfn_location;
static vfs_status delete_lfn_entries(fat32 *f, uint32_t directory_cluster,
                                     uint32_t target_sector,
                                     uint16_t target_slot,
                                     const uint8_t shortn[11]) {
  lfn_location locations[20];
  size_t count = 0;
  uint32_t cluster = directory_cluster;
  for (uint32_t walked = 0; walked < f->clusters; ++walked) {
    for (uint32_t s = 0; s < f->spc; ++s) {
      uint32_t rel = f->data_start + (cluster - 2) * f->spc + s;
      vfs_status st = read_sector(f, rel, f->sector);
      if (st != VFS_OK)
        return st;
      for (uint16_t i = 0; i < 16; ++i) {
        uint8_t *entry = f->sector + i * 32;
        if (rel == target_sector && i == target_slot) {
          uint8_t checksum = lfn_checksum(shortn);
          for (size_t j = 0; j < count; ++j) {
            st = read_sector(f, locations[j].sector, f->sector);
            if (st != VFS_OK)
              return st;
            uint8_t prior[32];
            bytes_copy(prior, f->sector + locations[j].slot * 32, 32);
            if (prior[11] == 0x0f && prior[13] == checksum) {
              prior[0] = 0xe5;
              st = entry_write(f, locations[j].sector,
                               locations[j].slot, prior);
              if (st != VFS_OK)
                return st;
            }
          }
          return VFS_OK;
        }
        if (!entry[0])
          return VFS_CORRUPT;
        if (entry[0] == 0xe5) {
          count = 0;
        } else if (entry[11] == 0x0f) {
          if (count == 20)
            count = 0;
          locations[count++] = (lfn_location){rel, i};
        } else {
          count = 0;
        }
      }
    }
    uint32_t next;
    vfs_status st = next_cluster(f, cluster, &next);
    if (st != VFS_OK)
      return st;
    if (!next)
      return VFS_CORRUPT;
    cluster = next;
  }
  return VFS_CORRUPT;
}
static uint32_t entry_cluster(const uint8_t e[32]) {
  return (uint32_t)u16(e + 26) | ((uint32_t)u16(e + 20) << 16);
}
static vfs_status resolve_parent(fat32 *f, const char *path,
                                 uint32_t *parent_cluster,
                                 const char **leaf) {
  if (!f || !f->mounted || !path || path[0] != '/' || !path[1] ||
      !parent_cluster || !leaf)
    return VFS_INVALID;
  uint32_t current = f->root_cluster;
  const char *part = path + 1;
  for (;;) {
    size_t length = 0;
    while (part[length] && part[length] != '/') {
      if (++length > 255)
        return VFS_TOO_LARGE;
    }
    if (!length || (length == 1 && part[0] == '.') ||
        (length == 2 && part[0] == '.' && part[1] == '.'))
      return VFS_INVALID;
    if (!part[length]) {
      *parent_cluster = current;
      *leaf = part;
      return VFS_OK;
    }
    if (!part[length + 1])
      return VFS_INVALID;
    char component[256];
    bytes_copy(component, part, length);
    component[length] = 0;
    uint32_t sector;
    uint16_t slot;
    uint8_t entry[32];
    vfs_status s = find_entry(f, current, component, &sector, &slot, entry);
    if (s != VFS_OK)
      return s;
    if (!(entry[11] & 0x10))
      return VFS_NOT_DIRECTORY;
    current = entry_cluster(entry);
    if (current < 2 || current >= f->clusters + 2)
      return VFS_CORRUPT;
    part += length + 1;
  }
}
static vfs_status initialize_directory(fat32 *f, uint32_t cluster,
                                       uint32_t parent) {
  uint8_t sector[512] = {0};
  bytes_copy(sector, ".          ", 11);
  sector[11] = 0x10;
  p16(sector + 26, (uint16_t)cluster);
  p16(sector + 20, (uint16_t)(cluster >> 16));
  uint8_t *dotdot = sector + 32;
  bytes_copy(dotdot, "..         ", 11);
  dotdot[11] = 0x10;
  p16(dotdot + 26, (uint16_t)parent);
  p16(dotdot + 20, (uint16_t)(parent >> 16));
  return write_sector(f, f->data_start + (cluster - 2) * f->spc, sector);
}
static vfs_status flookup(void *c, const char *p, vfs_node *out) {
  fat32 *f = c;
  if (!f || !f->mounted || !out)
    return VFS_INVALID;
  if (bytes_equal(p, "/", 2)) {
    *out = (vfs_node){VFS_DIRECTORY, 0, 0};
    return VFS_OK;
  }
  if (p[0] != '/')
    return VFS_INVALID;
  uint32_t directory_cluster, sec;
  uint16_t slot;
  uint8_t e[32];
  const char *leaf;
  vfs_status s = resolve_parent(f, p, &directory_cluster, &leaf);
  if (s != VFS_OK)
    return s;
  s = find_entry(f, directory_cluster, leaf, &sec, &slot, e);
  if (s != VFS_OK)
    return s;
  if (e[11] & 0x10) {
    *out = (vfs_node){VFS_DIRECTORY, 0, 0};
    return VFS_OK;
  }
  *out = (vfs_node){VFS_FILE, u32(e + 28), 0};
  return VFS_OK;
}
static vfs_status f_list(void *c, const char *p, size_t index,
                        vfs_dirent *out) {
  fat32 *f = c;
  if (!f || !f->mounted || !p || p[0] != '/' || !out)
    return VFS_INVALID;
  uint32_t directory = f->root_cluster;
  if (!bytes_equal(p, "/", 2)) {
    uint32_t parent, sector;
    uint16_t slot;
    uint8_t entry[32];
    const char *leaf;
    vfs_status status = resolve_parent(f, p, &parent, &leaf);
    if (status != VFS_OK)
      return status;
    status = find_entry(f, parent, leaf, &sector, &slot, entry);
    if (status != VFS_OK)
      return status;
    if (!(entry[11] & 0x10))
      return VFS_NOT_DIRECTORY;
    directory = entry_cluster(entry);
    if (directory < 2 || directory >= f->clusters + 2)
      return VFS_CORRUPT;
  }
  uint32_t cluster = directory;
  uint16_t lfn[260];
  for (size_t i = 0; i < 260; ++i)
    lfn[i] = 0xffff;
  uint8_t checksum = 0;
  unsigned expected = 0;
  size_t seen = 0;
  for (uint32_t walked = 0; walked < f->clusters; ++walked) {
    for (uint32_t s = 0; s < f->spc; ++s) {
      uint32_t rel = f->data_start + (cluster - 2) * f->spc + s;
      vfs_status status = read_sector(f, rel, f->sector);
      if (status != VFS_OK)
        return status;
      for (uint16_t i = 0; i < 16; ++i) {
        const uint8_t *entry = f->sector + i * 32;
        if (!entry[0])
          return VFS_NOT_FOUND;
        if (entry[0] == 0xe5) {
          expected = 0;
          continue;
        }
        if (entry[11] == 0x0f) {
          unsigned ordinal = entry[0] & 0x1f;
          if (entry[0] & 0x40) {
            for (size_t j = 0; j < 260; ++j)
              lfn[j] = 0xffff;
            expected = ordinal;
            checksum = entry[13];
          }
          if (!ordinal || ordinal > 20 || ordinal != expected ||
              entry[12] || entry[13] != checksum) {
            expected = 0;
            continue;
          }
          static const uint8_t offsets[] = {1, 3, 5, 7, 9, 14, 16,
                                            18, 20, 22, 24, 28, 30};
          for (unsigned j = 0; j < 13; ++j)
            lfn[(ordinal - 1) * 13 + j] = u16(entry + offsets[j]);
          --expected;
          continue;
        }
        if (entry[11] & 0x08) {
          expected = 0;
          continue;
        }
        char name[256];
        size_t length = 0;
        if (!expected && checksum == lfn_checksum(entry)) {
          while (length < 255 && lfn[length] && lfn[length] != 0xffff) {
            uint16_t ch = lfn[length];
            name[length] = ch < 0x80 ? (char)ch : '?';
            ++length;
          }
        }
        if (!length) {
          char shortbuf[13];
          short_name(entry, shortbuf);
          while (shortbuf[length] && length < sizeof(name) - 1) {
            name[length] = shortbuf[length];
            ++length;
          }
        }
        name[length] = 0;
        expected = 0;
        if (!length || bytes_equal(name, ".", 2) ||
            bytes_equal(name, "..", 3))
          continue;
        if (seen++ != index)
          continue;
        bytes_copy(out->name, name, length + 1);
        out->kind = (entry[11] & 0x10) ? VFS_DIRECTORY : VFS_FILE;
        out->size = u32(entry + 28);
        return VFS_OK;
      }
    }
    uint32_t next;
    vfs_status status = next_cluster(f, cluster, &next);
    if (status != VFS_OK)
      return status;
    if (!next)
      return VFS_NOT_FOUND;
    cluster = next;
  }
  return VFS_CORRUPT;
}
static vfs_status f_read(void *c, const char *p, uint64_t off, void *b,
                         size_t n, size_t *done) {
  fat32 *f = c;
  vfs_node node;
  vfs_status st = flookup(f, p, &node);
  if (st != VFS_OK)
    return st;
  if (node.kind != VFS_FILE)
    return VFS_IS_DIRECTORY;
  *done = 0;
  if (off >= node.size)
    return VFS_OK;
  uint32_t directory_cluster, sec;
  uint16_t slot;
  uint8_t e[32];
  const char *leaf;
  st = resolve_parent(f, p, &directory_cluster, &leaf);
  if (st != VFS_OK)
    return st;
  st = find_entry(f, directory_cluster, leaf, &sec, &slot, e);
  if (st != VFS_OK)
    return st;
  uint32_t cluster = u16(e + 26) | ((uint32_t)u16(e + 20) << 16);
  st = validate_file_chain(f, cluster, node.size);
  if (st != VFS_OK)
    return st;
  uint64_t skip = off / ((uint64_t)f->spc * 512);
  if (skip >= f->clusters)
    return VFS_CORRUPT;
  for (uint64_t i = 0; i < skip; i++) {
    if (!cluster)
      return VFS_CORRUPT;
    st = next_cluster(f, cluster, &cluster);
    if (st != VFS_OK)
      return st;
  }
  size_t remain = n;
  if (remain > node.size - off)
    remain = (size_t)(node.size - off);
  uint64_t within = off % ((uint64_t)f->spc * 512);
  while (remain) {
    if (!cluster || *done / ((uint64_t)f->spc * 512) >= f->clusters)
      return VFS_CORRUPT;
    uint32_t in_sector = (uint32_t)(within / 512),
             byte = (uint32_t)(within % 512);
    uint32_t rel = f->data_start + (cluster - 2) * f->spc + in_sector;
    st = read_sector(f, rel, f->sector);
    if (st != VFS_OK)
      return st;
    size_t take = 512 - byte;
    if (take > remain)
      take = remain;
    bytes_copy((uint8_t *)b + *done, f->sector + byte, take);
    *done += take;
    remain -= take;
    within += take;
    if (within == (uint64_t)f->spc * 512) {
      within = 0;
      st = next_cluster(f, cluster, &cluster);
      if (st != VFS_OK)
        return st;
    }
  }
  return VFS_OK;
}
static vfs_status file_cluster(fat32 *f, uint32_t *head, uint64_t index,
                               int create, uint32_t *out) {
  if (index >= f->clusters)
    return VFS_CORRUPT;
  if (!*head && create) {
    vfs_status s = allocate_cluster(f, head);
    if (s != VFS_OK)
      return s;
  }
  if (!*head) {
    *out = 0;
    return VFS_OK;
  }
  uint32_t c = *head;
  for (uint64_t i = 0; i < index; ++i) {
    uint32_t next;
    vfs_status s = next_cluster(f, c, &next);
    if (s != VFS_OK)
      return s;
    if (!next) {
      if (!create) {
        *out = 0;
        return VFS_OK;
      }
      s = allocate_cluster(f, &next);
      if (s != VFS_OK)
        return s;
      s = set_cluster(f, c, next);
      if (s != VFS_OK) {
        (void)set_cluster(f, next, 0);
        return s;
      }
    }
    c = next;
  }
  *out = c;
  return VFS_OK;
}
static vfs_status chain_tail(fat32 *f, uint32_t head, uint32_t *tail) {
  if (!head) {
    *tail = 0;
    return VFS_OK;
  }
  uint32_t current = head;
  for (uint32_t walked = 0; walked < f->clusters; ++walked) {
    uint32_t next;
    vfs_status s = next_cluster(f, current, &next);
    if (s != VFS_OK)
      return s;
    if (!next) {
      *tail = current;
      return VFS_OK;
    }
    current = next;
  }
  return VFS_CORRUPT;
}
static void rollback_extension(fat32 *f, uint32_t original_head,
                               uint32_t original_tail, uint32_t current_head) {
  if (!original_head) {
    if (current_head)
      (void)free_chain(f, current_head);
    return;
  }
  uint32_t first_added;
  if (next_cluster(f, original_tail, &first_added) != VFS_OK || !first_added)
    return;
  if (set_cluster(f, original_tail, 0x0fffffff) != VFS_OK)
    return;
  (void)free_chain(f, first_added);
}
static vfs_status f_write(void *c, const char *p, uint64_t off, const void *b,
                          size_t n, size_t *done) {
  fat32 *f = c;
  if (done)
    *done = 0;
  if (!f || !f->mounted || !done || (!b && n) || off > UINT32_MAX ||
      n > UINT32_MAX - off)
    return VFS_INVALID;
  uint32_t directory_cluster, rel;
  uint16_t slot;
  uint8_t e[32];
  const char *leaf;
  vfs_status s = resolve_parent(f, p, &directory_cluster, &leaf);
  if (s != VFS_OK)
    return s;
  s = find_entry(f, directory_cluster, leaf, &rel, &slot, e);
  if (s != VFS_OK)
    return s;
  if (e[11] & 0x10)
    return VFS_IS_DIRECTORY;
  uint32_t head = u16(e + 26) | ((uint32_t)u16(e + 20) << 16);
  s = validate_file_chain(f, head, u32(e + 28));
  if (s != VFS_OK)
    return s;
  if (off > u32(e + 28))
    return VFS_INVALID;
  uint32_t original_head = head, original_tail = 0;
  s = chain_tail(f, original_head, &original_tail);
  if (s != VFS_OK)
    return s;
  uint64_t pos = off;
  size_t left = n;
  while (left) {
    uint64_t ci = pos / ((uint64_t)f->spc * 512);
    uint32_t cl;
    s = file_cluster(f, &head, ci, 1, &cl);
    if (s != VFS_OK) {
      rollback_extension(f, original_head, original_tail, head);
      return s;
    }
    uint32_t sect = (uint32_t)((pos / 512) % f->spc),
             byte = (uint32_t)(pos % 512);
    uint32_t rs = f->data_start + (cl - 2) * f->spc + sect;
    s = read_sector(f, rs, f->sector);
    if (s != VFS_OK) {
      rollback_extension(f, original_head, original_tail, head);
      return s;
    }
    size_t take = 512 - byte;
    if (take > left)
      take = left;
    bytes_copy(f->sector + byte, (const uint8_t *)b + *done, take);
    s = write_sector(f, rs, f->sector);
    if (s != VFS_OK) {
      rollback_extension(f, original_head, original_tail, head);
      return s;
    }
    *done += take;
    pos += take;
    left -= take;
  }
  if (off + n > u32(e + 28))
    p32(e + 28, (uint32_t)(off + n));
  p16(e + 26, (uint16_t)head);
  p16(e + 20, (uint16_t)(head >> 16));
  s = entry_write(f, rel, slot, e);
  if (s != VFS_OK)
    rollback_extension(f, original_head, original_tail, head);
  return s;
}
static vfs_status f_create(void *c, const char *p, vfs_kind k) {
  fat32 *f = c;
  if (!f || !f->mounted || !p || p[0] != '/')
    return VFS_INVALID;
  if (k != VFS_FILE && k != VFS_DIRECTORY)
    return VFS_INVALID;
  uint32_t directory_cluster;
  const char *name;
  vfs_status s = resolve_parent(f, p, &directory_cluster, &name);
  if (s != VFS_OK)
    return s;
  uint32_t oldrel;
  uint16_t oldslot;
  uint8_t old[32];
  s = find_entry(f, directory_cluster, name, &oldrel, &oldslot, old);
  if (s == VFS_OK)
    return VFS_EXISTS;
  if (s != VFS_NOT_FOUND)
    return s;
  uint8_t sn[11];
  if (!make_short(name, sn)) {
    s = unique_short_alias(f, directory_cluster, name, sn);
    if (s != VFS_OK)
      return s;
  }
  uint32_t rel = 0;
  uint16_t slot = 0;
  uint32_t child = 0;
  if (k == VFS_DIRECTORY) {
    s = allocate_cluster(f, &child);
    if (s != VFS_OK)
      return s;
    s = initialize_directory(f, child, directory_cluster);
    if (s != VFS_OK) {
      (void)set_cluster(f, child, 0);
      return s;
    }
  }
  if (name_has_lfn(name, sn)) {
    s = write_lfn(f, directory_cluster, name, &rel, &slot, sn);
    if (s != VFS_OK) {
      if (child)
        (void)set_cluster(f, child, 0);
      return s;
    }
    if (child) {
      uint8_t entry[32] = {0};
      bytes_copy(entry, sn, 11);
      entry[11] = 0x10;
      p16(entry + 26, (uint16_t)child);
      p16(entry + 20, (uint16_t)(child >> 16));
      s = entry_write(f, rel, slot, entry);
      if (s != VFS_OK) {
        (void)set_cluster(f, child, 0);
        return s;
      }
    }
  } else {
    s = find_free_entry(f, directory_cluster, &rel, &slot);
    if (s != VFS_OK)
      return s;
    uint8_t entry[32] = {0};
    bytes_copy(entry, sn, 11);
    entry[11] = k == VFS_DIRECTORY ? 0x10 : 0x20;
    if (child) {
      p16(entry + 26, (uint16_t)child);
      p16(entry + 20, (uint16_t)(child >> 16));
    }
    s = entry_write(f, rel, slot, entry);
    if (s != VFS_OK) {
      if (child)
        (void)set_cluster(f, child, 0);
      return s;
    }
  }
  return VFS_OK;
}
static int dot_entry(const uint8_t entry[32], int parent) {
  const char *name = parent ? "..         " : ".          ";
  return bytes_equal(entry, name, 11);
}
static vfs_status directory_empty(fat32 *f, uint32_t first_cluster) {
  uint32_t cluster = first_cluster;
  for (uint32_t walked = 0; walked < f->clusters; ++walked) {
    if (cluster < 2 || cluster >= f->clusters + 2)
      return VFS_CORRUPT;
    for (uint32_t s = 0; s < f->spc; ++s) {
      uint32_t rel = f->data_start + (cluster - 2) * f->spc + s;
      vfs_status st = read_sector(f, rel, f->sector);
      if (st != VFS_OK)
        return st;
      for (uint16_t i = 0; i < 16; ++i) {
        uint8_t *entry = f->sector + i * 32;
        if (!entry[0])
          return VFS_OK;
        if (entry[0] == 0xe5 || dot_entry(entry, 0) || dot_entry(entry, 1))
          continue;
        return VFS_BUSY;
      }
    }
    uint32_t next;
    vfs_status st = next_cluster(f, cluster, &next);
    if (st != VFS_OK)
      return st;
    if (!next)
      return VFS_OK;
    cluster = next;
  }
  return VFS_CORRUPT;
}
static vfs_status free_chain(fat32 *f, uint32_t first_cluster) {
  uint32_t cluster = first_cluster;
  uint32_t count = 0;
  while (cluster) {
    if (++count > f->clusters)
      return VFS_CORRUPT;
    vfs_status st = next_cluster(f, cluster, &cluster);
    if (st != VFS_OK)
      return st;
  }
  cluster = first_cluster;
  while (cluster) {
    uint32_t next;
    vfs_status st = next_cluster(f, cluster, &next);
    if (st != VFS_OK)
      return st;
    st = set_cluster(f, cluster, 0);
    if (st != VFS_OK)
      return st;
    cluster = next;
  }
  return VFS_OK;
}
static vfs_status validate_file_chain(fat32 *f, uint32_t first_cluster,
                                      uint64_t size) {
  uint64_t cluster_bytes = (uint64_t)f->spc * 512;
  uint64_t required = size / cluster_bytes + !!(size % cluster_bytes);
  uint32_t current = first_cluster, slow = first_cluster, fast = first_cluster;
  uint32_t count = 0;
  while (current) {
    if (++count > f->clusters)
      return VFS_CORRUPT;
    vfs_status st = next_cluster(f, current, &current);
    if (st != VFS_OK)
      return st;
    if (slow) {
      st = next_cluster(f, slow, &slow);
      if (st != VFS_OK)
        return st;
    }
    for (unsigned i = 0; i < 2 && fast; ++i) {
      st = next_cluster(f, fast, &fast);
      if (st != VFS_OK)
        return st;
    }
    if (slow && fast && slow == fast)
      return VFS_CORRUPT;
  }
  return count < required ? VFS_CORRUPT : VFS_OK;
}
static vfs_status f_remove(void *c, const char *p) {
  fat32 *f = c;
  if (!f || !p || p[0] != '/')
    return VFS_INVALID;
  uint32_t directory_cluster, rel;
  uint16_t slot;
  uint8_t e[32];
  const char *leaf;
  vfs_status s = resolve_parent(f, p, &directory_cluster, &leaf);
  if (s != VFS_OK)
    return s;
  s = find_entry(f, directory_cluster, leaf, &rel, &slot, e);
  if (s != VFS_OK)
    return s;
  uint32_t cl = entry_cluster(e);
  if (e[11] & 0x10) {
    s = directory_empty(f, cl);
    if (s != VFS_OK)
      return s;
  }
  if (cl) {
    s = validate_file_chain(f, cl, (e[11] & 0x10) ? 0 : u32(e + 28));
    if (s != VFS_OK)
      return s;
  }
  s = delete_lfn_entries(f, directory_cluster, rel, slot, e);
  if (s != VFS_OK)
    return s;
  e[0] = 0xe5;
  s = entry_write(f, rel, slot, e);
  if (s != VFS_OK || !cl)
    return s;
  return free_chain(f, cl);
}
static vfs_status f_rename(void *c, const char *a, const char *b) {
  fat32 *f = c;
  uint32_t source_directory, destination_directory, rel;
  uint16_t slot;
  const char *source_name, *destination_name;
  vfs_status s = resolve_parent(f, a, &source_directory, &source_name);
  if (s != VFS_OK)
    return s;
  s = resolve_parent(f, b, &destination_directory, &destination_name);
  if (s != VFS_OK)
    return s;
  uint8_t old[32];
  s = find_entry(f, source_directory, source_name, &rel, &slot, old);
  if (s != VFS_OK)
    return s;
  if (old[11] & 0x10)
    return VFS_UNSUPPORTED;
  s = f_create(f, b, VFS_FILE);
  if (s != VFS_OK)
    return s;
  uint32_t nr;
  uint16_t ns;
  uint8_t fresh[32];
  s = find_entry(f, destination_directory, destination_name, &nr, &ns, fresh);
  if (s != VFS_OK)
    return s;
  bytes_copy(fresh + 11, old + 11, 21);
  s = entry_write(f, nr, ns, fresh);
  if (s != VFS_OK) {
    (void)f_remove(f, b);
    return s;
  }
  s = delete_lfn_entries(f, source_directory, rel, slot, old);
  if (s != VFS_OK)
    return s;
  old[0] = 0xe5;
  return entry_write(f, rel, slot, old);
}
static vfs_status f_sync(void *c) {
  fat32 *f = c;
  block_request r;
  block_request_init(&r, BLOCK_FLUSH, 0, 0, 0, 0, 0);
  return block_io_sync(f->device, &r, 10000000) == BLOCK_OK ? VFS_OK
                                                            : VFS_IO_ERROR;
}
static void f_destroy(void *c) {
  fat32 *f = c;
  if (!f)
    return;
  if (f->sector)
    f->memory.deallocate(f->memory.context, f->sector);
  f->mounted = 0;
}
const vfs_ops fat32_operations = {flookup,  f_create, f_read, f_write,
                                  f_remove, f_rename, f_sync, f_destroy,
                                  f_list};
vfs_status fat32_mount(fat32 *f, block_device *d, uint64_t start,
                       uint32_t sectors, const vfs_memory *m) {
  if (!f || !d || !m || !m->allocate || !m->deallocate ||
      d->logical_block_size != 512 || !sectors || start > d->capacity_blocks ||
      sectors > d->capacity_blocks - start)
    return VFS_INVALID;
  *f = (fat32){
      .device = d, .memory = *m, .start_lba = start, .sectors = sectors};
  f->sector = m->allocate(m->context, 512);
  if (!f->sector)
    return VFS_NO_MEMORY;
  vfs_status s = read_sector(f, 0, f->sector);
  if (s != VFS_OK)
    goto fail;
  uint8_t *b = f->sector;
  uint8_t saved_bpb[79];
  bytes_copy(saved_bpb, b + 11, sizeof(saved_bpb));
  uint32_t bps = u16(b + 11), spc = b[13], reserved = u16(b + 14), fats = b[16],
           total = u16(b + 19);
  if (!total)
    total = u32(b + 32);
  uint32_t fatsz = u32(b + 36), root = u32(b + 44);
  if (b[510] != 0x55 || b[511] != 0xaa || bps != 512 || !spc ||
      (spc & (spc - 1)) || spc > 128 || !reserved || (fats != 1 && fats != 2) ||
      u16(b + 17) || u16(b + 22) || u16(b + 42) || !fatsz || total > sectors ||
      total < reserved + fats * fatsz || root < 2) {
    s = VFS_CORRUPT;
    goto fail;
  }
  uint32_t data = reserved + fats * fatsz, clusters = (total - data) / spc;
  if (clusters < 65525 || clusters > 0x0ffffff5 || root >= clusters + 2 ||
      ((uint64_t)(clusters + 2) * 4 > (uint64_t)fatsz * 512)) {
    s = VFS_CORRUPT;
    goto fail;
  }
  uint16_t flags = u16(b + 40);
  uint16_t fsinfo_sector = u16(b + 48), backup_sector = u16(b + 50);
  if (backup_sector >= reserved || fsinfo_sector >= reserved) {
    s = VFS_CORRUPT;
    goto fail;
  }
  s = read_sector(f, backup_sector, f->sector);
  if (s != VFS_OK || f->sector[510] != 0x55 || f->sector[511] != 0xaa ||
      !bytes_equal(f->sector + 11, saved_bpb, sizeof(saved_bpb))) {
    s = VFS_CORRUPT;
    goto fail;
  }
  s = read_sector(f, fsinfo_sector, f->sector);
  if (s != VFS_OK || u32(f->sector) != 0x41615252 ||
      u32(f->sector + 484) != 0x61417272 ||
      u32(f->sector + 508) != 0xaa550000) {
    s = VFS_CORRUPT;
    goto fail;
  }
  if (flags & 0x80) {
    if ((flags & 0xf) != 0 && ((flags & 0xf) >= fats)) {
      s = VFS_CORRUPT;
      goto fail;
    }
    f->active_fat = flags & 0xf;
    f->mirrored = 0;
  } else
    f->mirrored = 1;
  f->fat_sectors = fatsz;
  f->fat_count = fats;
  f->reserved = reserved;
  f->data_start = data;
  f->clusters = clusters;
  f->root_cluster = root;
  f->spc = spc;
  f->mounted = 1;
  uint32_t n;
  s = next_cluster(f, root, &n);
  if (s != VFS_OK)
    goto fail;
  s = VFS_OK;
  return s;
fail:
  f_destroy(f);
  return s;
}
