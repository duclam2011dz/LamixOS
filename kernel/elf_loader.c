/* SPDX-License-Identifier: GPL-3.0-only */
#include "elf_loader.h"

#define ELF_PT_LOAD 1u
#define ELF_PT_DYNAMIC 2u
#define ELF_PT_INTERP 3u
#define ELF_PF_X 1u
#define ELF_PF_W 2u
#define ELF_PF_R 4u
#define ELF_DT_NULL 0
#define ELF_DT_RELA 7
#define ELF_DT_RELASZ 8
#define ELF_DT_RELAENT 9
#define ELF_DT_RELACOUNT 0x6ffffff9
#define ELF_DT_HASH 4
#define ELF_DT_STRTAB 5
#define ELF_DT_SYMTAB 6
#define ELF_DT_STRSZ 10
#define ELF_DT_SYMENT 11
#define ELF_DT_SYMBOLIC 16
#define ELF_DT_DEBUG 21
#define ELF_DT_FLAGS 30
#define ELF_DT_GNU_HASH 0x6ffffef5
#define ELF_DT_FLAGS_1 0x6ffffffb
#define ELF_DF_SYMBOLIC 2u
#define ELF_DF_1_PIE 0x08000000u
#define ELF_R_X86_64_RELATIVE 8u

typedef struct elf_phdr {
  uint32_t type, flags;
  uint64_t offset, vaddr, filesz, memsz, align;
} elf_phdr;

static uint16_t read16(const uint8_t *p) {
  return (uint16_t)p[0] | (uint16_t)((uint16_t)p[1] << 8);
}
static uint32_t read32(const uint8_t *p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
         ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static uint64_t read64(const uint8_t *p) {
  return (uint64_t)read32(p) | ((uint64_t)read32(p + 4) << 32);
}
static void write64(uint8_t *p, uint64_t v) {
  for (unsigned i = 0; i < 8; ++i)
    p[i] = (uint8_t)(v >> (i * 8));
}
static int add_ok(uint64_t a, uint64_t b, uint64_t *out) {
  if (b > UINT64_MAX - a)
    return 0;
  *out = a + b;
  return 1;
}
static int power2(uint64_t x) { return x && !(x & (x - 1)); }
static int file_range(size_t size, uint64_t offset, uint64_t length) {
  return offset <= size && length <= (uint64_t)size - offset;
}
static void zero(uint8_t *p, size_t n) {
  for (size_t i = 0; i < n; ++i)
    p[i] = 0;
}
static int covered(const lamix_elf_image *image, uint64_t address,
                   uint64_t length, uint32_t required_flags) {
  uint64_t end;
  if (!add_ok(address, length, &end))
    return 0;
  for (uint32_t i = 0; i < image->segment_count; ++i) {
    const lamix_elf_segment *s = &image->segments[i];
    uint64_t segment_end;
    if (add_ok(s->address, s->memory_size, &segment_end) &&
        address >= s->address && end <= segment_end &&
        (s->flags & required_flags) == required_flags)
      return 1;
  }
  return 0;
}

static lamix_elf_status parse_dynamic(const uint8_t *file, size_t file_size,
                                     const elf_phdr *dynamic,
                                     const elf_phdr *loads, size_t load_count,
                                     lamix_elf_image *image) {
  uint64_t rela = 0, relasz = 0, relaent = 0, relacount = 0;
  uint32_t rela_fields = 0;
  int terminated = 0;
  if (dynamic->filesz % 16 || dynamic->filesz > dynamic->memsz ||
      !file_range(file_size, dynamic->offset, dynamic->filesz))
    return LAMIX_ELF_INVALID;
  int dynamic_mapped = 0;
  for (size_t i = 0; i < load_count; ++i) {
    if (loads[i].type != ELF_PT_LOAD || dynamic->vaddr < loads[i].vaddr)
      continue;
    uint64_t delta = dynamic->vaddr - loads[i].vaddr;
    if (delta <= loads[i].filesz &&
        dynamic->offset == loads[i].offset + delta &&
        dynamic->filesz <= loads[i].filesz - delta)
      dynamic_mapped = 1;
  }
  if (!dynamic_mapped)
    return LAMIX_ELF_INVALID;
  for (uint64_t off = 0; off < dynamic->filesz; off += 16) {
    const uint8_t *d = file + dynamic->offset + off;
    int64_t tag = (int64_t)read64(d);
    uint64_t val = read64(d + 8);
    if (tag == ELF_DT_NULL) { terminated = 1; break; }
    if (tag == ELF_DT_RELA) {
      if (rela_fields & 1) return LAMIX_ELF_INVALID;
      rela_fields |= 1; rela = val;
    } else if (tag == ELF_DT_RELASZ) {
      if (rela_fields & 2) return LAMIX_ELF_INVALID;
      rela_fields |= 2; relasz = val;
    } else if (tag == ELF_DT_RELAENT) {
      if (rela_fields & 4) return LAMIX_ELF_INVALID;
      rela_fields |= 4; relaent = val;
    } else if (tag == ELF_DT_RELACOUNT) {
      if (rela_fields & 8) return LAMIX_ELF_INVALID;
      rela_fields |= 8; relacount = val;
    }
    else if (tag == ELF_DT_HASH || tag == ELF_DT_GNU_HASH ||
             tag == ELF_DT_STRTAB || tag == ELF_DT_SYMTAB) {
      int pointer_ok = 0;
      for (size_t i = 0; i < load_count; ++i)
        if (loads[i].type == ELF_PT_LOAD && val >= loads[i].vaddr &&
            val - loads[i].vaddr < loads[i].filesz)
          pointer_ok = 1;
      if (!pointer_ok) return LAMIX_ELF_INVALID;
    } else if (tag == ELF_DT_STRSZ || tag == ELF_DT_SYMENT) {
      if (!val) return LAMIX_ELF_INVALID;
    } else if (tag == ELF_DT_SYMBOLIC) {
      if (val) return LAMIX_ELF_INVALID;
    } else if (tag == ELF_DT_DEBUG) {
      if (val) return LAMIX_ELF_UNSUPPORTED;
    } else if (tag == ELF_DT_FLAGS) {
      if (val & ~ELF_DF_SYMBOLIC) return LAMIX_ELF_UNSUPPORTED;
    } else if (tag == ELF_DT_FLAGS_1) {
      if (val & ~ELF_DF_1_PIE) return LAMIX_ELF_UNSUPPORTED;
    }
    else return LAMIX_ELF_UNSUPPORTED;
  }
  if (!terminated || (!relasz && (rela || relaent || relacount)) ||
      (relasz && (relaent != 24 || relasz % 24 || relacount > relasz / 24)))
    return LAMIX_ELF_INVALID;
  if (!relasz)
    return LAMIX_ELF_OK;
  uint64_t table_file = UINT64_MAX;
  for (size_t i = 0; i < load_count; ++i) {
    uint64_t end;
    if (loads[i].type == ELF_PT_LOAD && rela >= loads[i].vaddr &&
        add_ok(rela, relasz, &end) &&
        end <= loads[i].vaddr + loads[i].filesz) {
      table_file = loads[i].offset + (rela - loads[i].vaddr);
      break;
    }
  }
  if (table_file == UINT64_MAX || !file_range(file_size, table_file, relasz))
    return LAMIX_ELF_INVALID;
  for (uint64_t off = 0; off < relasz; off += 24) {
    const uint8_t *r = file + table_file + off;
    uint64_t target = read64(r), info = read64(r + 8);
    int64_t addend = (int64_t)read64(r + 16);
    uint64_t biased_target;
    if (!add_ok(image->base, target, &biased_target) ||
        (uint32_t)info != ELF_R_X86_64_RELATIVE || (info >> 32) != 0 ||
        !covered(image, biased_target, 8, ELF_PF_W))
      return LAMIX_ELF_UNSUPPORTED;
    uint64_t value;
    if (addend < 0) {
      uint64_t magnitude = (uint64_t)(-(addend + 1)) + 1;
      if (magnitude > image->base)
        return LAMIX_ELF_INVALID;
      value = image->base - magnitude;
    } else if (!add_ok(image->base, (uint64_t)addend, &value)) {
      return LAMIX_ELF_INVALID;
    }
    uint64_t image_start = image->base + image->minimum_vaddr;
    uint64_t image_end = image_start + image->size;
    if (value >= LAMIX_ELF_USER_LIMIT ||
        (value && (value < image_start || value > image_end)))
      return LAMIX_ELF_INVALID;
    write64(image->bytes + (biased_target - image->base - image->minimum_vaddr), value);
  }
  return LAMIX_ELF_OK;
}

lamix_elf_status lamix_elf_prepare(const void *source, size_t file_size,
                                    uint64_t load_base,
                                    const lamix_elf_allocator *allocator,
                                    lamix_elf_image *image) {
  const uint8_t *file = source;
  elf_phdr headers[64];
  size_t header_count = 0, load_count = 0;
  uint64_t phoff, entry, minimum = UINT64_MAX, maximum = 0;
  const elf_phdr *dynamic = 0;
  if (!file || !allocator || !allocator->allocate || !allocator->deallocate ||
      !image || file_size < 64 || load_base < LAMIX_ELF_LOAD_BASE ||
      load_base >= LAMIX_ELF_USER_LIMIT)
    return LAMIX_ELF_INVALID;
  *image = (lamix_elf_image){0};
  if (file[0] != 0x7f || file[1] != 'E' || file[2] != 'L' || file[3] != 'F' ||
      file[4] != 2 || file[5] != 1 || file[6] != 1 || read16(file + 16) != 3 ||
      read16(file + 18) != 62 || read32(file + 20) != 1 ||
      read16(file + 52) != 64 || read16(file + 54) != 56)
    return LAMIX_ELF_INVALID;
  entry = read64(file + 24);
  phoff = read64(file + 32);
  uint16_t phnum = read16(file + 56);
  if (!phnum || phnum > 64 || !file_range(file_size, phoff,
                                           (uint64_t)phnum * 56))
    return LAMIX_ELF_INVALID;
  for (uint16_t i = 0; i < phnum; ++i) {
    const uint8_t *p = file + phoff + (uint64_t)i * 56;
    elf_phdr h = {read32(p), read32(p + 4), read64(p + 8), read64(p + 16),
                  read64(p + 32), read64(p + 40), read64(p + 48)};
    uint64_t end;
    if (h.type == ELF_PT_INTERP)
      return LAMIX_ELF_UNSUPPORTED;
    if (h.type == ELF_PT_DYNAMIC) {
      if (dynamic)
        return LAMIX_ELF_INVALID;
      dynamic = &headers[header_count];
    } else if (h.type != ELF_PT_LOAD && h.type != 4 && h.type != 0x6474e551)
      return LAMIX_ELF_UNSUPPORTED;
    if (h.type == ELF_PT_LOAD) {
      if (++load_count > LAMIX_ELF_MAX_SEGMENTS || !h.memsz ||
          h.filesz > h.memsz || !file_range(file_size, h.offset, h.filesz) ||
          !add_ok(h.vaddr, h.memsz, &end) || (h.flags & ~(ELF_PF_R|ELF_PF_W|ELF_PF_X)) ||
          !(h.flags & ELF_PF_R) || ((h.flags & (ELF_PF_W|ELF_PF_X)) == (ELF_PF_W|ELF_PF_X)) ||
          (h.align > 1 && (!power2(h.align) || (h.vaddr % h.align) != (h.offset % h.align))) ||
          (h.vaddr & 4095) || (h.offset & 4095))
        return LAMIX_ELF_INVALID;
      if (h.vaddr < minimum) minimum = h.vaddr;
      if (end > maximum) maximum = end;
    }
    headers[header_count++] = h;
  }
  uint64_t rounded_maximum;
  if (maximum > UINT64_MAX - 4095)
    return LAMIX_ELF_INVALID;
  rounded_maximum = (maximum + 4095) & ~UINT64_C(4095);
  if (!load_count || minimum == UINT64_MAX || maximum <= minimum ||
      rounded_maximum - minimum > LAMIX_ELF_MAX_SPAN ||
      (minimum & 4095) || entry < minimum || entry >= maximum)
    return LAMIX_ELF_INVALID;
  if (load_base < minimum)
    return LAMIX_ELF_INVALID;
  uint64_t load_end;
  if (!add_ok(load_base, rounded_maximum - minimum, &load_end) ||
      load_end > LAMIX_ELF_USER_LIMIT)
    return LAMIX_ELF_INVALID;
  size_t image_size = (size_t)(rounded_maximum - minimum);
  uint8_t *staging = allocator->allocate(allocator->context, image_size);
  if (!staging)
    return LAMIX_ELF_NO_MEMORY;
  zero(staging, image_size);
  image->allocator = *allocator;
  image->bytes = staging;
  image->size = image_size;
  image->base = load_base - minimum;
  image->minimum_vaddr = minimum;
  image->entry = image->base + entry;
  for (size_t i = 0; i < header_count; ++i) {
    const elf_phdr *h = &headers[i];
    if (h->type != ELF_PT_LOAD)
      continue;
    uint64_t end = h->vaddr + h->memsz;
    uint64_t page_start = h->vaddr & ~UINT64_C(4095);
    uint64_t page_end = (end + 4095) & ~UINT64_C(4095);
    for (size_t j = 0; j < image->segment_count; ++j) {
      const lamix_elf_segment *prior = &image->segments[j];
      uint64_t prior_start = prior->address - image->base;
      uint64_t prior_end = (prior_start + prior->memory_size + 4095) & ~UINT64_C(4095);
      if (page_start < prior_end && page_end > prior_start) {
        lamix_elf_release(image);
        return LAMIX_ELF_INVALID;
      }
    }
    lamix_elf_segment *s = &image->segments[image->segment_count++];
    s->address = image->base + h->vaddr;
    s->offset = h->vaddr - minimum;
    s->file_size = h->filesz;
    s->memory_size = h->memsz;
    s->flags = h->flags;
    for (uint64_t j = 0; j < h->filesz; ++j)
      staging[s->offset + j] = file[h->offset + j];
  }
  if (entry < minimum || !covered(image, image->entry, 1, ELF_PF_X)) {
    lamix_elf_release(image);
    return LAMIX_ELF_INVALID;
  }
  if (dynamic) {
    lamix_elf_status status = parse_dynamic(file, file_size, dynamic,
                                            headers, header_count, image);
    if (status != LAMIX_ELF_OK) {
      lamix_elf_release(image);
      return status;
    }
  }
  return LAMIX_ELF_OK;
}

void lamix_elf_release(lamix_elf_image *image) {
  if (!image)
    return;
  if (image->bytes && image->allocator.deallocate)
    image->allocator.deallocate(image->allocator.context, image->bytes);
  *image = (lamix_elf_image){0};
}
