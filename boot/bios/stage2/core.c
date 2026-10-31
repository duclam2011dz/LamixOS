/* SPDX-License-Identifier: GPL-3.0-only */
#include "division_check.h"
#include "lamix/boot/loader.h"
#include "lamix/boot/acpi_rsdp.h"
#include <stddef.h>
#include <stdint.h>

#define PAGE 4096u
#define STATE ((volatile uint32_t *)(uintptr_t)0x5000)
#define MAX_RESERVATIONS 128u
#define MAX_MAP 1400u
#define NX UINT64_C(0x8000000000000000)
extern uint8_t __core_start[], __core_writable[], __core_end[];
extern void stage2_enter64(void), stage2_fault32(void);
extern uint32_t stage2_faults64[256];
/* ASM offsets: CR3/NX/RSP/BootInfo/entry/unused/IDTR/NMI. */
uint8_t enter_context[56] __attribute__((aligned(8)));
static lbp_memory raw[256], normalized[MAX_MAP], scratch[MAX_MAP];
static lbp_reservation reservations[MAX_RESERVATIONS];
static lbp_segment mappings[128], kernel_segments[32];
static uint32_t raw_count, res_count, map_count, kernel_count, normalized_count;
static uint8_t gpt_array[16384];
static lamix_disk disk;
static lamix_fat fat;
static lamix_elf_plan elf;
static uint8_t idt32[2048];
static uint32_t pool, pool_used, nx_enabled, io_failed;
static uint8_t acpi_bytes[36];
static uint32_t acpi_physical, acpi_length, acpi_revision;
#ifdef LAMIX_M16_VIDEO
static lbp_framebuffer video_framebuffer;
static uint32_t have_framebuffer;
static uint16_t vbe_version;
#endif
static void serial(const char *p);
static void hex(uint64_t n);

static void bytes(void *to, const void *from, uint32_t n) {
  uint8_t *a = to;
  const uint8_t *b = from;
  for (uint32_t i = 0; i < n; i++)
    a[i] = b[i];
}
static void clear(void *to, uint32_t n) {
  uint8_t *a = to;
  for (uint32_t i = 0; i < n; i++)
    a[i] = 0;
}
static void wr32(void *to, uint32_t n) { bytes(to, &n, 4); }
static void wr64(void *to, uint64_t n) { bytes(to, &n, 8); }
static void out8(uint16_t p, uint8_t v) {
  __asm__ volatile("outb %0,%1" ::"a"(v), "Nd"(p));
}
static uint8_t in8(uint16_t p) {
  uint8_t v;
  __asm__ volatile("inb %1,%0" : "=a"(v) : "Nd"(p));
  return v;
}
static int rsdp_at(uint32_t physical) {
  const uint8_t *p = (const uint8_t *)(uintptr_t)physical;
  uint32_t length, revision;
  if (!lamix_rsdp_validate(p, 36, &length, &revision)) return 0;
  uint32_t rsdt = (uint32_t)p[16] | ((uint32_t)p[17] << 8) |
                  ((uint32_t)p[18] << 16) | ((uint32_t)p[19] << 24);
  if (revision == 0) {
    if (!rsdt) return 0;
  } else {
    uint64_t xsdt = 0;
    for (uint32_t i = 0; i < 8; ++i) xsdt |= (uint64_t)p[24 + i] << (8 * i);
    if (!xsdt && !rsdt) return 0;
  }
  bytes(acpi_bytes, p, length);
  acpi_physical = physical;
  acpi_length = length;
  acpi_revision = revision;
  return 1;
}
static void discover_acpi(void) {
  uint16_t ebda_segment;
  __asm__ volatile("movw 0x40e, %0" : "=r"(ebda_segment) : : "memory");
  uint32_t ebda = (uint32_t)ebda_segment << 4;
  if (ebda >= 0x80000 && ebda < 0xa0000) {
    uint32_t end = ebda + 1024;
    if (end > 0xa0000) end = 0xa0000;
    for (uint32_t p = ebda; p + 36 <= end; p += 16)
      if (rsdp_at(p)) return;
  }
  for (uint32_t p = 0xe0000; p + 36 <= 0x100000; p += 16)
    if (rsdp_at(p)) return;
}
#ifdef LAMIX_M16_VIDEO
static int bios_video_call(uint16_t ax, uint16_t bx, uint16_t cx, uint16_t dx,
                           uint16_t es, uint16_t di) {
  *(volatile uint8_t *)(uintptr_t)0x5080 = 1;
  *(volatile uint16_t *)(uintptr_t)0x5082 = ax;
  *(volatile uint16_t *)(uintptr_t)0x5084 = bx;
  *(volatile uint16_t *)(uintptr_t)0x5086 = cx;
  *(volatile uint16_t *)(uintptr_t)0x5088 = dx;
  *(volatile uint16_t *)(uintptr_t)0x508a = es;
  *(volatile uint16_t *)(uintptr_t)0x508c = di;
  int (*thunk)(void) = (void *)(uintptr_t)STATE[8];
  int success = thunk();
  *(volatile uint8_t *)(uintptr_t)0x5080 = 0;
  return success && *(volatile uint16_t *)(uintptr_t)0x508e == 0x004f;
}
static uint16_t read16(const uint8_t *p) {
  return (uint16_t)p[0] | (uint16_t)((uint16_t)p[1] << 8);
}
static uint32_t read32(const uint8_t *p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
         ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static uint32_t vbe_mask(uint8_t bits, uint8_t position) {
  if (!bits || bits > 32 || position >= 32 || bits + position > 32)
    return 0;
  uint64_t value = bits == 32 ? UINT32_MAX : (UINT64_C(1) << bits) - 1;
  return (uint32_t)(value << position);
}
static int contiguous_mask(uint32_t mask) {
  if (!mask) return 0;
  while (!(mask & 1u)) mask >>= 1;
  return (mask & (mask + 1u)) == 0;
}
static int video_range_safe(const lbp_framebuffer *fb) {
  uint64_t end = fb->physical + fb->length;
  if (end < fb->physical) return 0;
  for (uint32_t i = 0; i < normalized_count; ++i) {
    uint64_t base = normalized[i].base;
    uint64_t limit = base + normalized[i].length;
    if (base < end && fb->physical < limit &&
        (normalized[i].type == LBP_USABLE ||
         normalized[i].type == LBP_LOADER_RECLAIM))
      return 0;
  }
  for (uint32_t i = 0; i < res_count; ++i) {
    uint64_t base = reservations[i].base;
    uint64_t limit = base + reservations[i].length;
    if (base < end && fb->physical < limit) return 0;
  }
  return 1;
}
static int video_mode(uint16_t mode, uint32_t wanted_width,
                      uint32_t wanted_height, lbp_framebuffer *out) {
  if (!bios_video_call(0x4f01, 0, mode, 0, 0, 0x5900)) return 0;
  const uint8_t *info = (const uint8_t *)(uintptr_t)0x5900;
  uint16_t attributes = read16(info);
  uint32_t width = read16(info + 18), height = read16(info + 20);
  uint32_t pitch = read16(info + (vbe_version >= 0x0300 ? 50 : 16));
  uint32_t bpp = info[25];
  uint32_t red = vbe_mask(info[vbe_version >= 0x0300 ? 54 : 31],
                          info[vbe_version >= 0x0300 ? 55 : 32]);
  uint32_t green = vbe_mask(info[vbe_version >= 0x0300 ? 56 : 33],
                            info[vbe_version >= 0x0300 ? 57 : 34]);
  uint32_t blue = vbe_mask(info[vbe_version >= 0x0300 ? 58 : 35],
                           info[vbe_version >= 0x0300 ? 59 : 36]);
  uint8_t reserved_size = info[vbe_version >= 0x0300 ? 60 : 37];
  uint32_t reserved = reserved_size
                          ? vbe_mask(reserved_size,
                                     info[vbe_version >= 0x0300 ? 61 : 38])
                          : 0;
  uint32_t physical = read32(info + 40);
  if ((attributes & 0x91) != 0x91 || info[27] != 6 || bpp != 32 ||
      width != wanted_width || height != wanted_height ||
      pitch < width * 4 || (pitch & 3) || !physical || (physical & 4095) ||
      !contiguous_mask(red) || !contiguous_mask(green) ||
      !contiguous_mask(blue) || (reserved && !contiguous_mask(reserved)) ||
      (red & green) || (red & blue) || (green & blue) ||
      (reserved & (red | green | blue)))
    return 0;
  uint64_t bytes = (uint64_t)pitch * height;
  if (!bytes || bytes > UINT32_MAX - 4095 ||
      bytes > UINT64_MAX - 4095 || physical > UINT32_MAX - bytes)
    return 0;
  uint32_t length = (uint32_t)((bytes + 4095) & ~UINT64_C(4095));
  if (physical > UINT32_MAX - length) return 0;
  *out = (lbp_framebuffer){{LBP_FRAMEBUFFER, 0, sizeof(lbp_framebuffer)},
                           physical, length, width, height, pitch, bpp,
                           red, green, blue, reserved};
  return 1;
}
static void discover_vbe(void) {
  uint8_t *controller = (uint8_t *)(uintptr_t)0x5800;
  clear(controller, 512);
  controller[0] = 'V'; controller[1] = 'B';
  controller[2] = 'E'; controller[3] = '2';
  if (!bios_video_call(0x4f00, 0, 0, 0, 0, 0x5800) ||
      read32(controller) != UINT32_C(0x41534556) || read16(controller + 4) < 0x0200)
    return;
  vbe_version = read16(controller + 4);
  uint32_t list = ((uint32_t)read16(controller + 16) << 4) +
                  read16(controller + 14);
  if (list < 0x400 || list > 0x100000 - 512) return;
  const uint16_t *modes = (const uint16_t *)(uintptr_t)list;
  const uint32_t widths[2] = {1024, 800}, heights[2] = {768, 600};
  for (unsigned preference = 0; preference < 2; ++preference) {
    for (unsigned i = 0; i < 256 && modes[i] != 0xffff; ++i) {
      lbp_framebuffer candidate;
      if (!video_mode(modes[i], widths[preference], heights[preference],
                      &candidate) || !video_range_safe(&candidate))
        continue;
      if (!bios_video_call(0x4f02, (uint16_t)(modes[i] | 0x4000), 0, 0, 0, 0))
        continue;
      video_framebuffer = candidate;
      have_framebuffer = 1;
      serial("M16:VBE:MODE=");
      serial(candidate.width == 1024 ? "1024x768\n" : "800x600\n");
      serial("M16:VBE:BASE="); hex(candidate.physical); serial(" LENGTH=");
      hex(candidate.length); serial(" PITCH="); hex(candidate.pitch); serial("\n");
      return;
    }
  }
  serial("M16:VBE:UNAVAILABLE\n");
}
#endif
static void serial(const char *p) {
  for (; *p; p++) {
    for (uint32_t n = 65535; n && !(in8(0x3fd) & 32); n--) {
    }
    out8(0x3f8, (uint8_t)*p);
  }
}
__attribute__((noreturn)) static void fail(const char *reason) {
  serial("M05:FAIL\n");
  serial(io_failed ? "DISK" : reason);
  serial("\n");
  for (;;)
    __asm__ volatile("cli; hlt");
}
void stage2_exception32(void) { fail("EXCEPTION"); }
static uint32_t round_pages(uint64_t n) {
  if (!n || n > UINT32_MAX - 4095)
    fail("MEMORY");
  return (uint32_t)((n + 4095) & ~UINT64_C(4095));
}
static void reserve(uint32_t base, uint32_t size, uint32_t purpose) {
  if (res_count == MAX_RESERVATIONS || !base || base % PAGE || !size ||
      size % PAGE || (uint64_t)base + size > UINT64_C(0x100000000))
    fail("MEMORY");
  for (uint32_t i = 0; i < res_count; i++)
    if ((uint64_t)base < reservations[i].base + reservations[i].length &&
        reservations[i].base < (uint64_t)base + size)
      fail("MEMORY");
  reservations[res_count++] = (lbp_reservation){base, size, purpose,
                                                purpose <= 2   ? 0
                                                : purpose == 3 ? 1
                                                               : 2,
                                                0};
}
static uint32_t allocate(uint32_t size, uint32_t purpose) {
  for (uint32_t i = 0; i < normalized_count; i++) {
    const lbp_memory *m = normalized + i;
    if (m->type != LBP_USABLE)
      continue;
    uint64_t at = m->base < 0x100000 ? 0x100000 : m->base,
             end = m->base + m->length;
    if (end > UINT64_C(0x100000000))
      end = UINT64_C(0x100000000);
    while (at < end && size <= end - at) {
      uint64_t next = at;
      for (uint32_t j = 0; j < res_count; j++)
        if (at < reservations[j].base + reservations[j].length &&
            reservations[j].base < at + size) {
          uint64_t e = reservations[j].base + reservations[j].length;
          if (e > next)
            next = e;
        }
      if (next == at) {
        reserve((uint32_t)at, size, purpose);
        clear((void *)(uintptr_t)at, size);
        return (uint32_t)at;
      }
      at = next;
    }
  }
  fail("MEMORY");
}
static int kernel_allocate(void *context, uint32_t size, uint64_t *physical,
                           void **writable) {
  (void)context;
  uint32_t p = allocate(size, LBP_PURPOSE_KERNEL);
  *physical = p;
  *writable = (void *)(uintptr_t)p;
  return 1;
}
static void mapping(uint32_t p, uint64_t v, uint32_t size,
                    uint32_t permissions) {
  if (!size)
    return;
  if (map_count == 128)
    fail("MEMORY");
  mappings[map_count++] = (lbp_segment){p, v, size, permissions, 0};
}
static uint32_t table_page(void) {
  if (pool_used == 128)
    fail("MEMORY");
  return pool + PAGE * pool_used++;
}
static void map_page(uint64_t virtual_address, uint32_t physical,
                     uint32_t perms) {
  uint64_t *table = (void *)(uintptr_t)pool;
  for (unsigned shift = 39; shift > 12; shift -= 9) {
    uint32_t i = (uint32_t)((virtual_address >> shift) & 511);
    if (!(table[i] & 1)) {
      uint32_t page = table_page();
      table[i] = (uint64_t)page | 3;
    }
    table = (void *)(uintptr_t)(uint32_t)(table[i] & UINT64_C(0xfffff000));
  }
  uint32_t i = (uint32_t)((virtual_address >> 12) & 511);
  if (table[i] & 1)
    fail("ENTRY");
  table[i] = (uint64_t)physical | 1 | ((perms & LBP_WRITE) ? 2 : 0) |
             ((nx_enabled && !(perms & LBP_EXECUTE)) ? NX : 0)
#ifdef LAMIX_M16_VIDEO
             | (have_framebuffer && physical >= video_framebuffer.physical &&
                        physical < video_framebuffer.physical + video_framebuffer.length
                    ? UINT64_C(0x18) : 0)
#endif
      ;
}
static void identity(uint32_t p, uint32_t size, uint32_t perms) {
  mapping(p, p, size, perms);
}

static void cpu(void) {
  uint32_t a, b, c, d, original, changed;
  __asm__ volatile("pushfl; popl %0" : "=r"(original));
  changed = original ^ (1u << 21);
  __asm__ volatile("pushl %0; popfl" ::"r"(changed) : "cc");
  __asm__ volatile("pushfl; popl %0" : "=r"(changed));
  __asm__ volatile("pushl %0; popfl" ::"r"(original) : "cc");
  if (!((original ^ changed) & (1u << 21)))
    fail("CPU");
  __asm__ volatile("cpuid"
                   : "=a"(a), "=b"(b), "=c"(c), "=d"(d)
                   : "a"(1), "c"(0));
  if ((d & ((1u << 5) | (1u << 6))) != ((1u << 5) | (1u << 6)))
    fail("CPU");
  __asm__ volatile("cpuid"
                   : "=a"(a), "=b"(b), "=c"(c), "=d"(d)
                   : "a"(0x80000000), "c"(0));
  if (a < 0x80000001)
    fail("CPU");
  __asm__ volatile("cpuid"
                   : "=a"(a), "=b"(b), "=c"(c), "=d"(d)
                   : "a"(0x80000001), "c"(0));
  if (!(d & (1u << 29)))
    fail("CPU");
  nx_enabled = !!(d & (1u << 20));
}
static void early_idt(void) {
  uint32_t handler = (uint32_t)(uintptr_t)stage2_fault32;
  for (unsigned i = 0; i < 256; i++) {
    uint8_t *e = idt32 + i * 8;
    e[0] = (uint8_t)handler;
    e[1] = (uint8_t)(handler >> 8);
    e[2] = 8;
    e[5] = 0x8e;
    e[6] = (uint8_t)(handler >> 16);
    e[7] = (uint8_t)(handler >> 24);
  }
  uint8_t ptr[6];
  uint16_t limit = sizeof(idt32) - 1;
  bytes(ptr, &limit, 2);
  wr32(ptr + 2, (uint32_t)(uintptr_t)idt32);
  __asm__ volatile("lidt %0" ::"m"(ptr) : "memory");
  out8(0x70, *(volatile uint8_t *)(uintptr_t)0x5018);
}
static int disk_read(void *ctx, uint64_t offset, void *dest, uint32_t size) {
  (void)ctx;
  uint8_t *to = dest;
  if (offset > (uint64_t)STATE[3] * 512 ||
      size > (uint64_t)STATE[3] * 512 - offset)
    return 0;
  while (size) {
    uint32_t skip = (uint32_t)offset & 511, part = 32768 - skip;
    if (part > size)
      part = size;
    *(volatile uint32_t *)(uintptr_t)0x5040 = (uint32_t)(offset / 512);
    *(volatile uint32_t *)(uintptr_t)0x5044 = (skip + part + 511) / 512;
    int (*thunk)(void) = (void *)(uintptr_t)STATE[8];
    if (!thunk()) {
      io_failed = 1;
      return 0;
    }
    bytes(to, (const void *)(uintptr_t)(0x8000 + skip), part);
    to += part;
    offset += part;
    size -= part;
  }
  return 1;
}
static uint64_t normalize(const lbp_reservation *rs, uint32_t count) {
  uint64_t needed = 0;
  if (lbp_normalize(raw, raw_count, rs, count, scratch, MAX_MAP, normalized,
                    MAX_MAP, &needed) != LBP_OK)
    fail("MEMORY");
  return needed;
}
static void memory(void) {
  uint64_t n = 0;
  if (!STATE[4] || STATE[4] > 256 ||
      lbp_decode_e820((void *)(uintptr_t)0x18000, STATE[4] * 24, 24, raw, 256,
                      &n) != LBP_OK)
    fail("E820");
  raw_count = (uint32_t)n;
  normalized_count = (uint32_t)normalize(0, 0);
  reserve(0x5000, 0xb000, LBP_PURPOSE_LOADER);
  reserve(0x10000, 0x10000, LBP_PURPOSE_LOADER);
  reserve(0x20000, round_pages((uintptr_t)__core_end - 0x20000),
          LBP_PURPOSE_LOADER);
}
static void final_tables(uint32_t stack, uint32_t info) {
  uint8_t *gdt = (void *)(uintptr_t)(pool + PAGE),
          *idt = (void *)(uintptr_t)(pool + 2 * PAGE),
          *tss = (void *)(uintptr_t)(pool + 4 * PAGE);
  uint64_t descriptors[8] = {0,
                             UINT64_C(0x00cf9a000000ffff),
                             UINT64_C(0x00cf92000000ffff),
                             0,
                             0,
                             UINT64_C(0x00af9a000000ffff),
                             0,
                             0};
  uint32_t tssbase = pool + 4 * PAGE;
  descriptors[6] = 103 | ((uint64_t)(tssbase & 0xffffff) << 16) |
                   UINT64_C(0x0000890000000000) |
                   ((uint64_t)(tssbase >> 24) << 56);
  bytes(gdt, descriptors, sizeof descriptors);
  uint32_t emergency = allocate(8192, LBP_PURPOSE_STACK);
  identity(emergency, 8192, LBP_READ | LBP_WRITE);
  wr64(tss + 36, (uint64_t)emergency + 8192);
  tss[102] = 104;
  for (unsigned i = 0; i < 256; i++) {
    uint8_t *e = idt + i * 16;
    uint32_t h = stage2_faults64[i];
    e[0] = (uint8_t)h;
    e[1] = (uint8_t)(h >> 8);
    e[2] = 0x28;
    e[4] = 1;
    e[5] = 0x8e;
    e[6] = (uint8_t)(h >> 16);
    e[7] = (uint8_t)(h >> 24);
  }
  uint8_t gdtr[6];
  uint16_t lim = 63;
  bytes(gdtr, &lim, 2);
  wr32(gdtr + 2, pool + PAGE);
  __asm__ volatile("lgdt %0" ::"m"(gdtr) : "memory");
  wr32(enter_context, pool);
  wr32(enter_context + 4, nx_enabled);
  wr64(enter_context + 8, (uint64_t)stack + 65536 - 8);
  wr64(enter_context + 16, info);
  wr64(enter_context + 24, elf.entry);
  enter_context[40] = 0xff;
  enter_context[41] = 0x0f;
  wr64(enter_context + 42, (uint64_t)pool + 2 * PAGE);
  enter_context[50] = *(volatile uint8_t *)(uintptr_t)0x5018;
}
static uint8_t *record(uint8_t *at, uint32_t type, uint32_t size,
                       uint32_t prefix, uint32_t count) {
  clear(at, size);
  lbp_record *r = (void *)at;
  *r = (lbp_record){type, LBP_REQUIRED, size};
  if (type <= 4) {
    uint32_t v = 1, stride = 32;
    bytes(at + prefix - 16, &v, 4);
    bytes(at + prefix - 12, &stride, 4);
    wr64(at + prefix - 8, count);
  }
  return at + prefix;
}
static uint32_t bootinfo(uint32_t physical, uint32_t stack) {
  uint8_t *blob = (void *)(uintptr_t)physical, *at = blob + 64;
  normalized_count = (uint32_t)normalize(reservations, res_count);
  /* Policy bootstrap: low RAM không được cấp phát bởi kernel. Tách tại 1MiB
   * nếu cần để không làm mất RAM an toàn phía trên. */
  for (uint32_t i = 0; i < normalized_count; i++)
    if (normalized[i].base < 0x100000 && normalized[i].type == LBP_USABLE) {
      lbp_memory *m = normalized + i;
      if (m->base + m->length > 0x100000) {
        if (normalized_count == MAX_MAP)
          fail("MEMORY");
        for (uint32_t j = normalized_count; j > i + 1; j--)
          normalized[j] = normalized[j - 1];
        normalized[i + 1] = *m;
        normalized[i + 1].base = 0x100000;
        normalized[i + 1].length -= 0x100000 - m->base;
        normalized_count++;
        m->length = 0x100000 - m->base;
      }
      m->type = LBP_RESERVED;
    }
  uint32_t size = 32 + normalized_count * 32;
  bytes(record(at, 1, size, 32, normalized_count), normalized,
        normalized_count * 32);
  at += size;
  size = 40 + kernel_count * 32;
  uint8_t *r = at;
  bytes(record(at, 2, size, 40, kernel_count), kernel_segments,
        kernel_count * 32);
  wr64(r + 16, elf.entry);
  at += size;
  size = 32 + res_count * 32;
  bytes(record(at, 3, size, 32, res_count), reservations, res_count * 32);
  at += size;
  size = 72 + map_count * 32;
  r = at;
  bytes(record(at, 4, size, 72, map_count), mappings, map_count * 32);
  wr64(r + 16, pool);
  wr64(r + 24, stack);
  wr64(r + 32, 65536);
  wr64(r + 40, (uint64_t)stack + 65536 - 8);
  wr64(r + 48, stack - PAGE);
  at += size;
  r = at;
  record(at, 5, 80, 80, 0);
  wr32(r + 16, LBP_SOURCE_BIOS);
  wr32(r + 20, 512);
  bytes(r + 24, disk.disk_guid, 16);
  bytes(r + 40, disk.esp_guid, 16);
  bytes(r + 56, disk.system_guid, 16);
  wr32(r + 72, LBP_SLOT_A);
  at += 80;
  if (acpi_length) {
    r = at;
    record(at, LBP_ACPI, sizeof(lbp_acpi), sizeof(lbp_acpi), 0);
    wr64(r + 16, acpi_physical);
    wr32(r + 24, acpi_length);
    wr32(r + 28, acpi_revision);
    at += sizeof(lbp_acpi);
  }
#ifdef LAMIX_M16_VIDEO
  if (have_framebuffer) {
    record(at, LBP_FRAMEBUFFER, sizeof(video_framebuffer),
           sizeof(video_framebuffer), 0);
    ((lbp_record *)(void *)at)->flags = 0;
    bytes(at, &video_framebuffer, sizeof(video_framebuffer));
    at += sizeof(video_framebuffer);
  }
#endif
  uint64_t provided = nx_enabled ? LBP_FEATURE_NX : 0;
  if (acpi_length) provided |= LBP_FEATURE_ACPI;
#ifdef LAMIX_M16_VIDEO
  if (have_framebuffer) provided |= LBP_FEATURE_FRAMEBUFFER;
#endif
  lbp_header header = {{'L', 'A', 'M', 'I', 'X', 'L', 'B', 'P'},
                       1,
                       0,
                       64,
                       (uint64_t)(at - blob),
                       elf.required_features,
                       provided,
                       64,
                       5 + !!acpi_length
#ifdef LAMIX_M16_VIDEO
                           + !!have_framebuffer
#endif
                       ,
                       0,
                       physical};
  bytes(blob, &header, 64);
#ifdef LAMIX_TEST_LBP_CORRUPT
  blob[0] = 0;
#endif
#ifdef LAMIX_TEST_MAP_CORRUPT
  /* Tìm record ENTRY, biến mapping đầu thành RWX để validator chặn. */
  uint8_t *bad = blob + 64;
  for (unsigned i = 0; i < 3; i++)
    bad += (uint32_t)((lbp_record *)(void *)bad)->size;
  wr32(bad + 72 + 24, 7);
#endif
  lbp_view view;
  lbp_span acpi_span = {acpi_physical, acpi_bytes, acpi_length};
  lbp_status validation = lbp_validate(blob, header.total_size,
                                       acpi_length ? &acpi_span : 0,
                                       acpi_length ? 1 : 0, &view);
  if (validation != LBP_OK) {
    serial("M16:LBP:STATUS="); hex(validation); serial("\n");
    fail("LBP");
  }
  return (uint32_t)header.total_size;
}
static void hex(uint64_t n) {
  static const char digits[] = "0123456789abcdef";
  for (unsigned i = 16; i; i--) {
    char text[2] = {digits[(n >> ((i - 1) * 4)) & 15], 0};
    serial(text);
  }
}
static void report(void) {
  for (uint32_t i = 0; i < normalized_count; i++) {
    serial("M05:MEMORY ");
    hex(normalized[i].base);
    serial(" ");
    hex(normalized[i].length);
    serial(" ");
    hex(normalized[i].type);
    serial("\n");
  }
  for (uint32_t i = 0; i < res_count; i++) {
    serial("M05:RESERVE ");
    hex(reservations[i].base);
    serial(" ");
    hex(reservations[i].length);
    serial(" ");
    hex(reservations[i].purpose);
    serial("\n");
  }
  for (uint32_t i = 0; i < map_count; i++) {
    serial("M05:MAP ");
    hex(mappings[i].physical);
    serial(" ");
    hex(mappings[i].virtual_address);
    serial(" ");
    hex(mappings[i].length);
    serial(" ");
    hex(mappings[i].permissions);
    serial("\n");
  }
}
void stage2_main(void) {
  *(volatile uint8_t *)(uintptr_t)0x5080 = 0;
  *(volatile uint16_t *)(uintptr_t)0x508e = 0;
  early_idt();
  cpu();
  if (!division_check())
    fail("S2");
  memory();
  discover_acpi();
#ifdef LAMIX_M16_VIDEO
  discover_vbe();
#endif
  lamix_reader drive = {0, disk_read, (uint64_t)STATE[3] * 512};
  if (lamix_gpt_open(drive, STATE[1], STATE[2], gpt_array, sizeof gpt_array,
                     &disk) != LBP_OK)
    fail("GPT");
  if (lamix_fat_open(&disk, &fat) != LBP_OK)
    fail("FAT");
  lamix_reader file = {&fat, lamix_fat_read, fat.file_size};
  uint64_t available_features =
      (nx_enabled ? LBP_FEATURE_NX : 0) |
      (acpi_length ? LBP_FEATURE_ACPI : 0);
#ifdef LAMIX_M16_VIDEO
  if (have_framebuffer) available_features |= LBP_FEATURE_FRAMEBUFFER;
#endif
  lbp_status status = lamix_elf_inspect(file, available_features, &elf);
  if (status != LBP_OK)
    fail(status == LBP_UNSUPPORTED ? "NOTE" : "ELF");
  status = lamix_elf_load(file, &elf, kernel_allocate, 0, kernel_segments, 32);
  if (status != LBP_OK)
    fail(status == LBP_TRUNCATED ? "DISK" : "MEMORY");
  kernel_count = elf.count;
  for (uint32_t i = 0; i < kernel_count; i++)
    mapping((uint32_t)kernel_segments[i].physical,
            kernel_segments[i].virtual_address,
            (uint32_t)kernel_segments[i].length,
            kernel_segments[i].permissions);
  uint32_t info = allocate(1048576, LBP_PURPOSE_BOOTINFO);
  uint32_t guarded = allocate(PAGE + 65536, LBP_PURPOSE_STACK),
           stack = guarded + PAGE;
  pool = allocate(128 * PAGE, LBP_PURPOSE_TABLES);
  pool_used = 5;
  identity(0x6000, 0x2000, LBP_READ | LBP_WRITE);
  uint32_t writable = (uint32_t)(uintptr_t)__core_writable,
           core_end = (uint32_t)(uintptr_t)__core_end;
  identity(0x20000, writable - 0x20000, LBP_READ | LBP_EXECUTE);
  identity(writable, core_end - writable, LBP_READ | LBP_WRITE);
  identity(info, 1048576, LBP_READ);
  identity(stack, 65536, LBP_READ | LBP_WRITE);
  identity(pool, 128 * PAGE, LBP_READ | LBP_WRITE);
#ifdef LAMIX_M16_VIDEO
  if (have_framebuffer)
    identity((uint32_t)video_framebuffer.physical,
             (uint32_t)video_framebuffer.length, LBP_READ | LBP_WRITE);
#endif
  final_tables(stack, info);
  (void)bootinfo(info, stack);
  for (uint32_t i = 0; i < map_count; i++)
    for (uint32_t j = 0; j < mappings[i].length; j += PAGE)
      map_page(mappings[i].virtual_address + j,
               (uint32_t)mappings[i].physical + j, mappings[i].permissions);
  report();
  serial("M05:HANDOFF\n");
  stage2_enter64();
  fail("ENTRY");
}
