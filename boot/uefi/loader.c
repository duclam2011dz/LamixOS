/* SPDX-License-Identifier: GPL-3.0-only */
#include "lamix/boot/loader.h"
#include "lamix/boot/acpi_rsdp.h"
#include "efi.h"
#include "policy.h"

#define PAGE 4096u
#define MAX_ALLOC 128u
#define MAX_RAW 2048u
#define MAX_MAP 10000u
#define MAX_RS 128u
#define EFI_MEMORY_LOADER_DATA 2u
#define EFI_MEMORY_BOOT_SERVICES_DATA 4u
#define EFI_MEMORY_CONVENTIONAL 7u
#define EFI_MEMORY_ACPI_RECLAIM 9u
#define EFI_MEMORY_ACPI_NVS 10u
#define EFI_MEMORY_RUNTIME UINT64_C(0x8000000000000000)
#define EFI_FILE_INFO_GUID_INIT                                                \
  {                                                                            \
    0x09576e92, 0x6d3f, 0x11d2, {                                              \
      0x8e, 0x39, 0, 0xa0, 0xc9, 0x69, 0x72, 0x3b                              \
    }                                                                          \
  }

EFI_GUID g_loaded_image_guid = {
    0x5b1b31a1, 0x9562, 0x11d2, {0x8e, 0x3f, 0, 0xa0, 0xc9, 0x69, 0x72, 0x3b}};
EFI_GUID g_simple_fs_guid = {
    0x964e5b22, 0x6459, 0x11d2, {0x8e, 0x39, 0, 0xa0, 0xc9, 0x69, 0x72, 0x3b}};
EFI_GUID g_gop_guid = {0x9042a9de,
                       0x23dc,
                       0x4a38,
                       {0x96, 0xfb, 0x7a, 0xde, 0xd0, 0x80, 0x51, 0x6a}};
EFI_GUID g_acpi2_guid = {
    0x8868e871, 0xe4f1, 0x11d3, {0xbc, 0x22, 0, 0x80, 0xc7, 0x3c, 0x88, 0x81}};
EFI_GUID g_acpi_guid = {
    0xeb9d2d30, 0x2d88, 0x11d3, {0x9a, 0x16, 0, 0x90, 0x27, 0x3f, 0xc1, 0x4d}};

typedef EFI_STATUS(__attribute__((ms_abi)) *
                   EFI_FILE_OPEN)(EFI_FILE *, EFI_FILE **, CHAR16 *, uint64_t,
                                  uint64_t);
typedef EFI_STATUS(__attribute__((ms_abi)) * EFI_FILE_CLOSE)(EFI_FILE *);
typedef EFI_STATUS(__attribute__((ms_abi)) *
                   EFI_LOCATE_HANDLE_BUFFER)(uint32_t, EFI_GUID *, void *,
                                             uint64_t *, EFI_HANDLE **);
typedef struct {
  uint32_t version;
  uint32_t horizontal, vertical, format;
  uint32_t masks[4];
  uint32_t pixels_per_scanline;
} GOP_INFO;
typedef struct {
  uint32_t max_mode, mode;
  GOP_INFO *info;
  uint64_t info_size, fb_base, fb_size;
} GOP_MODE;
typedef struct {
  EFI_STATUS(__attribute__((ms_abi)) * query_mode)
  (void *, uint32_t, uint64_t *, GOP_INFO **);
  EFI_STATUS(__attribute__((ms_abi)) * set_mode)(void *, uint32_t);
  void *blt;
  GOP_MODE *mode;
} GOP;
typedef struct {
  uint32_t media_id;
  uint8_t removable, present, logical, read_only, write_caching, pad[3];
  uint32_t block_size, io_align;
  uint64_t last_block;
} BLOCK_MEDIA;
typedef struct {
  uint64_t revision;
  BLOCK_MEDIA *media;
  EFI_STATUS(__attribute__((ms_abi)) * reset)(void *, uint8_t);
  EFI_STATUS(__attribute__((ms_abi)) * read_blocks)
  (void *, uint32_t, uint64_t, uint64_t, void *);
  void *write_blocks, *flush;
} BLOCK_IO;
typedef struct {
  uint8_t type, subtype;
  uint16_t length;
} DEVICE_PATH_NODE;
static EFI_GUID g_block_io_guid = {
    0x964e5b21, 0x6459, 0x11d2, {0x8e, 0x39, 0, 0xa0, 0xc9, 0x69, 0x72, 0x3b}};
static EFI_GUID g_device_path_guid = {
    0x09576e91, 0x6d3f, 0x11d2, {0x8e, 0x39, 0, 0xa0, 0xc9, 0x69, 0x72, 0x3b}};

static EFI_BOOT_SERVICES *bs;
static EFI_HANDLE image;
static lbp_reservation reservations[MAX_RS];
static uint32_t reservation_count;
static lbp_memory raw[MAX_RAW], norm[MAX_MAP], scratch[MAX_MAP];
static lbp_segment kernel_segments[32], maps[256];
static uint8_t gpt_scratch[16384];
static uint8_t block_sector[512] __attribute__((aligned(4096)));
static uint32_t kernel_count, map_count;
static uint64_t allocations[MAX_ALLOC];
static uint32_t allocation_pages[MAX_ALLOC], allocation_count;
static uint64_t file_phys;
static uint32_t file_pages;
static uint8_t *file_bytes;
static uint64_t file_size;
static uint64_t root_phys, stack_phys, tables_phys, info_phys, trampoline_phys;
static uint32_t tables_pages;
static uint64_t kernel_entry, nx_enabled;
static uint64_t kernel_required_features;
static uint16_t uefi_cs;
extern uint8_t tramp_begin[], tramp_end[], tramp_gdt[], tramp_gdtr_base[],
    tramp_idtr_base[], tramp_idt[], tramp_stub_table[];
extern void __attribute__((ms_abi, noreturn))
efi_transition(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
static lbp_framebuffer framebuffer;
static lbp_acpi acpi_record;
static uint8_t acpi_bytes[36];
static lbp_device boot_device;
static int have_fb, have_acpi;
static EFI_MEMORY_DESCRIPTOR *map_buffer;
static uint64_t map_capacity;

static void out8(uint16_t p, uint8_t v) {
  __asm__ volatile("outb %0,%1" ::"a"(v), "Nd"(p));
}
static uint8_t in8(uint16_t p) {
  uint8_t v;
  __asm__ volatile("inb %1,%0" : "=a"(v) : "Nd"(p));
  return v;
}
static void serial_init(void) {
  out8(0x3f9, 0);
  out8(0x3fb, 0x80);
  out8(0x3f8, 1);
  out8(0x3f9, 0);
  out8(0x3fb, 3);
  out8(0x3fa, 0xc7);
  out8(0x3fc, 0x0b);
}
static void serial(const char *s) {
  for (; *s; s++) {
    for (uint32_t n = 65535; n && !(in8(0x3fd) & 32); n--) {
    }
    out8(0x3f8, (uint8_t)*s);
  }
}
__attribute__((noreturn)) static void fail(const char *s) {
  serial("M06:FAIL\n");
  serial(s);
  serial("\n");
  for (;;)
    __asm__ volatile("cli; hlt");
}
__attribute__((noreturn)) static void fail_quiet(void) {
  for (;;)
    __asm__ volatile("cli; hlt");
}
static void zero(void *p, uint64_t n) {
  uint8_t *b = p;
  while (n--)
    *b++ = 0;
}
static uint64_t round_pages(uint64_t n) {
  if (!n || n > UINT64_MAX - (PAGE - 1))
    fail("SIZE");
  return (n + PAGE - 1) & ~(uint64_t)(PAGE - 1);
}
static void reserve(uint64_t base, uint64_t size, uint32_t purpose) {
  if (!base || base % PAGE || !size || size % PAGE ||
      reservation_count == MAX_RS || base + size > UINT64_C(0x100000000))
    fail("MEMORY");
  for (uint32_t i = 0; i < reservation_count; i++)
    if (base < reservations[i].base + reservations[i].length &&
        reservations[i].base < base + size)
      fail("OVERLAP");
  reservations[reservation_count++] =
      (lbp_reservation){base, size, purpose,
                        purpose <= 2   ? LBP_RELEASE_NEVER
                        : purpose == 3 ? LBP_RELEASE_COPIED
                                       : LBP_RELEASE_REPLACED,
                        0};
}
static uint64_t alloc_pages(uint64_t bytes, uint32_t purpose) {
  uint64_t n = round_pages(bytes) / PAGE, addr = UINT32_MAX;
  if (n > UINT32_MAX || allocation_count == MAX_ALLOC)
    fail("ALLOC");
  EFI_STATUS s = bs->allocate_pages(EFI_ALLOCATE_MAX_ADDRESS,
                                    EFI_MEMORY_LOADER_DATA, n, &addr);
  if (EFI_ERROR(s) || addr == 0 || addr + n * PAGE > UINT64_C(0x100000000))
    fail("ALLOC");
  zero((void *)(uintptr_t)addr, n * PAGE);
  allocations[allocation_count] = addr;
  allocation_pages[allocation_count++] = (uint32_t)n;
  reserve(addr, n * PAGE, purpose);
  return addr;
}
static int elf_allocate(void *ctx, uint32_t bytes, uint64_t *physical,
                        void **writable) {
  (void)ctx;
  uint64_t p = alloc_pages(bytes, LBP_PURPOSE_KERNEL);
  *physical = p;
  *writable = (void *)(uintptr_t)p;
  return 1;
}
static int file_read(void *ctx, uint64_t offset, void *dest, uint32_t n) {
  (void)ctx;
  if (offset > file_size || n > file_size - offset)
    return 0;
  __builtin_memcpy(dest, file_bytes + offset, n);
  return 1;
}
typedef struct {
  BLOCK_IO *io;
} DISK_CONTEXT;
static int block_read(void *ctx, uint64_t offset, void *dest, uint32_t size) {
  DISK_CONTEXT *d = ctx;
  uint8_t *out = dest;
  if (d->io->media->last_block >= UINT64_MAX / 512)
    return 0;
  uint64_t disk_bytes = (d->io->media->last_block + 1) * 512;
  if (offset > disk_bytes || size > disk_bytes - offset)
    return 0;
  while (size) {
    uint64_t lba = offset / 512;
    uint32_t skip = (uint32_t)(offset % 512), part = 512 - skip;
    if (part > size)
      part = size;
    EFI_STATUS s = d->io->read_blocks(d->io, d->io->media->media_id, lba, 512,
                                      block_sector);
    if (EFI_ERROR(s))
      return 0;
    __builtin_memcpy(out, block_sector + skip, part);
    out += part;
    offset += part;
    size -= part;
  }
  return 1;
}
static void find_disk(EFI_LOADED_IMAGE *loaded) {
  void *loaded_path = 0;
  EFI_STATUS s = bs->handle_protocol(loaded->device_handle, &g_device_path_guid,
                                     &loaded_path);
  if (EFI_ERROR(s) || !loaded_path)
    fail("DEVICE_PATH");
  uint8_t *path = loaded_path, *hd = 0;
  uint32_t prefix = 0;
  for (uint32_t at = 0; at < 4096;) {
    DEVICE_PATH_NODE *node = (void *)(path + at);
    if (node->length < 4 || node->length > 4096 - at)
      fail("DEVICE_PATH");
    if (node->type == 4 && node->subtype == 1 && node->length == 42) {
      hd = (uint8_t *)node;
      prefix = at;
      break;
    }
    if (node->type == 0x7f)
      break;
    at += node->length;
  }
  if (!hd || hd[41] != 2)
    fail("ESP_DEVICE_PATH");
  uint64_t esp_start = *(uint64_t *)(hd + 8), esp_size = *(uint64_t *)(hd + 16);
  if (!esp_size || esp_start > UINT32_MAX || esp_size > UINT32_MAX ||
      esp_start + esp_size < esp_start)
    fail("ESP_EXTENT");
  EFI_HANDLE *handles = 0;
  uint64_t count = 0;
  s = bs->locate_handle_buffer(2, &g_block_io_guid, 0, &count, &handles);
  if (EFI_ERROR(s) || !count || !handles)
    fail("BLOCK_IO");
  int found = 0;
  for (uint64_t i = 0; i < count && !found; i++) {
    BLOCK_IO *io = 0;
    void *candidate_path = 0;
    if (EFI_ERROR(
            bs->handle_protocol(handles[i], &g_block_io_guid, (void **)&io)) ||
        !io || !io->media || io->media->logical || !io->media->present ||
        io->media->block_size != 512 || io->media->io_align > 4096 ||
        (io->media->io_align > 1 &&
         (io->media->io_align & (io->media->io_align - 1))) ||
        io->media->last_block >= UINT64_MAX / 512)
      continue;
    if (EFI_ERROR(bs->handle_protocol(handles[i], &g_device_path_guid,
                                      &candidate_path)) ||
        !candidate_path)
      continue;
    uint8_t *cp = candidate_path;
    DEVICE_PATH_NODE *end = (void *)(cp + prefix);
    if (__builtin_memcmp(cp, path, prefix) || end->type != 0x7f ||
        end->subtype != 0xff)
      continue;
    DISK_CONTEXT dc = {io};
    lamix_reader reader = {&dc, block_read, (io->media->last_block + 1) * 512};
    lamix_disk disk;
    if (lamix_gpt_open_uefi(reader, (uint32_t)esp_start, (uint32_t)esp_size,
                            gpt_scratch, sizeof gpt_scratch, &disk) != LBP_OK)
      continue;
    if (__builtin_memcmp(disk.esp_guid, hd + 24, 16))
      continue;
    boot_device =
        (lbp_device){{LBP_BOOT_DEVICE, LBP_REQUIRED, sizeof(lbp_device)},
                     LBP_SOURCE_UEFI,
                     512,
                     {0},
                     {0},
                     {0},
                     LBP_SLOT_A,
                     0};
    __builtin_memcpy(boot_device.disk_guid, disk.disk_guid, 16);
    __builtin_memcpy(boot_device.esp_guid, disk.esp_guid, 16);
    __builtin_memcpy(boot_device.system_guid, disk.system_guid, 16);
    found = 1;
  }
  bs->free_pool(handles);
  if (!found)
    fail("GPT_IDENTITY");
}
static void read_kernel(EFI_SYSTEM_TABLE *st) {
  EFI_LOADED_IMAGE *loaded = 0;
  EFI_GUID fsid = g_simple_fs_guid;
  EFI_STATUS s =
      bs->handle_protocol(image, &g_loaded_image_guid, (void **)&loaded);
  if (EFI_ERROR(s) || !loaded)
    fail("LOADED_IMAGE");
  find_disk(loaded);
  EFI_SIMPLE_FS *fs = 0;
  s = bs->handle_protocol(loaded->device_handle, &fsid, (void **)&fs);
  if (EFI_ERROR(s) || !fs)
    fail("ESP");
  EFI_FILE *root = 0, *f = 0;
  s = fs->open_volume(fs, &root);
  if (EFI_ERROR(s) || !root)
    fail("ESP");
  static const CHAR16 path[] = {'E', 'F',  'I', '\\', 'L', 'A', 'M', 'I',
                                'X', '\\', 'A', '\\', 'K', 'E', 'R', 'N',
                                'E', 'L',  '.', 'E',  'L', 'F', 0};
  s = ((EFI_FILE_OPEN)root->open)(root, &f, (CHAR16 *)path, EFI_FILE_MODE_READ,
                                  0);
  ((EFI_FILE_CLOSE)root->close)(root);
  if (EFI_ERROR(s) || !f)
    fail("KERNEL_FILE");
  EFI_GUID info_guid = EFI_FILE_INFO_GUID_INIT;
  uint64_t info_size = 0;
  s = ((EFI_STATUS(__attribute__((ms_abi)) *)(
      EFI_FILE *, EFI_GUID *, uint64_t *, void *))f->get_info)(f, &info_guid,
                                                               &info_size, 0);
  if (s != EFI_BUFFER_TOO_SMALL || info_size < 80 || info_size > 4096)
    fail("KERNEL_INFO");
  void *info = 0;
  if (EFI_ERROR(bs->allocate_pool(EFI_MEMORY_LOADER_DATA, info_size, &info)))
    fail("ALLOC");
  s = ((EFI_STATUS(__attribute__((ms_abi)) *)(EFI_FILE *, EFI_GUID *,
                                              uint64_t *, void *))f->get_info)(
      f, &info_guid, &info_size, info);
  if (EFI_ERROR(s))
    fail("KERNEL_INFO");
  file_size = *(uint64_t *)((uint8_t *)info + 8);
  bs->free_pool(info);
  if (file_size < 64 || file_size > 16 * 1024 * 1024)
    fail("KERNEL_SIZE");
  file_pages = (uint32_t)(round_pages(file_size) / PAGE);
  file_phys = alloc_pages(file_size, LBP_PURPOSE_LOADER);
  file_bytes = (uint8_t *)(uintptr_t)file_phys;
  uint64_t left = file_size, at = 0;
  while (left) {
    uint64_t part = left > 65536 ? 65536 : left;
    s = f->read(f, &part, file_bytes + at);
    if (EFI_ERROR(s) || !part)
      fail("KERNEL_READ");
    at += part;
    left -= part;
  }
  ((EFI_FILE_CLOSE)f->close)(f);
  (void)st;
}

static int valid_rsdp(uint8_t *candidate, uint32_t expected_revision) {
  uint32_t length, revision;
  return lamix_rsdp_validate(candidate, 36, &length, &revision) &&
         revision == expected_revision;
}

static int known_acpi(EFI_SYSTEM_TABLE *st) {
  uint8_t *candidate = 0;
  uint32_t len = 0;
  for (uint32_t pass = 0; pass < 2 && !candidate; ++pass) {
    EFI_GUID *wanted = pass == 0 ? &g_acpi2_guid : &g_acpi_guid;
    uint32_t revision = pass == 0 ? 2 : 0;
    for (uint64_t i = 0; i < st->table_count; ++i) {
      if (__builtin_memcmp(&st->tables[i].vendor_guid, wanted, 16)) continue;
      uint8_t *try = (uint8_t *)(uintptr_t)st->tables[i].vendor_table;
      if (valid_rsdp(try, revision)) {
        candidate = try;
        len = revision ? 36 : 20;
        break;
      }
    }
  }
  if (!candidate) return 0;
  __builtin_memcpy(acpi_bytes, candidate, len);
  acpi_record = (lbp_acpi){{LBP_ACPI, 0, sizeof(lbp_acpi)},
                           (uint64_t)(uintptr_t)candidate,
                           len,
                           candidate[15]};
  have_acpi = 1;
  return 1;
}
static void discover_fb(EFI_SYSTEM_TABLE *st) {
  GOP *g = 0;
  if (EFI_ERROR(bs->locate_protocol(&g_gop_guid, 0, (void **)&g)) || !g ||
      !g->mode || !g->mode->info)
    return;
  GOP_INFO *info = g->mode->info;
  lbp_framebuffer candidate;
  if (!lamix_uefi_framebuffer(info->horizontal, info->vertical, info->format,
                              info->masks, info->pixels_per_scanline,
                              g->mode->fb_base, g->mode->fb_size, &candidate))
    return;
  framebuffer = candidate;
  have_fb = 1;
  (void)st;
}

static uint32_t record(uint8_t *p, uint32_t type, uint32_t len, uint32_t prefix,
                       uint64_t count) {
  zero(p, len);
  lbp_record *r = (void *)p;
  *r = (lbp_record){type, LBP_REQUIRED, len};
  uint32_t v = 1, stride = 32;
  uint64_t count64 = count;
  __builtin_memcpy(p + prefix - 16, &v, 4);
  __builtin_memcpy(p + prefix - 12, &stride, 4);
  __builtin_memcpy(p + prefix - 8, &count64, 8);
  return prefix;
}
static void set_mapping(uint64_t physical, uint64_t virt, uint64_t length,
                        uint32_t perms) {
  if (map_count == 256)
    fail("MAPPINGS");
  maps[map_count++] = (lbp_segment){physical, virt, length, perms, 0};
}
static uint64_t table_page(uint32_t *used) {
  if (*used >= tables_pages)
    fail("TABLES");
  uint64_t p = tables_phys + (uint64_t)(*used)++ * PAGE;
  return p;
}
static uint64_t *next_table(uint64_t *entry, uint32_t *used) {
  if (!(*entry & 1))
    *entry = table_page(used) | 3;
  return (uint64_t *)(uintptr_t)(*entry & UINT64_C(0x000ffffffffff000));
}
static void map_one_attributes(uint64_t va, uint64_t pa, uint32_t perms,
                               uint64_t attributes, uint32_t *used) {
  uint64_t *t = (uint64_t *)(uintptr_t)root_phys;
  for (int s = 39; s > 12; s -= 9)
    t = next_table(&t[(va >> s) & 511], used);
  uint64_t *e = &t[(va >> 12) & 511];
  if (*e & 1)
    fail("MAP_OVERLAP");
  *e =
      pa | attributes | 1 | ((perms & LBP_WRITE) ? 2 : 0) |
      (nx_enabled && !(perms & LBP_EXECUTE) ? UINT64_C(0x8000000000000000) : 0);
}
static void map_one(uint64_t va, uint64_t pa, uint32_t perms, uint32_t *used) {
  map_one_attributes(va, pa, perms, 0, used);
}
static void map_span(uint64_t p, uint64_t v, uint64_t n, uint32_t perms,
                     uint32_t *used) {
  for (uint64_t off = 0; off < n; off += PAGE)
    map_one(v + off, p + off, perms, used);
  set_mapping(p, v, n, perms);
}
static void map_framebuffer_span(uint64_t p, uint64_t n, uint32_t *used) {
  for (uint64_t off = 0; off < n; off += PAGE)
    map_one_attributes(p + off, p + off, LBP_READ | LBP_WRITE, 0x18, used);
  set_mapping(p, p, n, LBP_READ | LBP_WRITE);
}
static void make_page_tables(void) {
  tables_pages = 128;
  tables_phys = alloc_pages((uint64_t)tables_pages * PAGE, LBP_PURPOSE_TABLES);
  root_phys = tables_phys;
  uint32_t used = 1;
  map_span(info_phys, info_phys, 1048576, LBP_READ, &used);
  map_span(stack_phys, stack_phys, 65536, LBP_READ | LBP_WRITE, &used);
  map_span(tables_phys, tables_phys, (uint64_t)tables_pages * PAGE,
           LBP_READ | LBP_WRITE, &used);
  map_span(trampoline_phys, trampoline_phys, 3 * PAGE, LBP_READ | LBP_EXECUTE,
           &used);
  for (uint32_t i = 0; i < kernel_count; i++)
    map_span(kernel_segments[i].physical, kernel_segments[i].virtual_address,
             kernel_segments[i].length, kernel_segments[i].permissions, &used);
  if (have_fb) {
    uint64_t end = framebuffer.physical + framebuffer.length;
    if (end < framebuffer.physical || end > UINT64_MAX - (PAGE - 1))
      fail("GOP");
    uint64_t first = framebuffer.physical & ~(uint64_t)(PAGE - 1),
             last = (end + PAGE - 1) & ~(uint64_t)(PAGE - 1);
    map_framebuffer_span(first, last - first, &used);
  }
}
static void prepare_trampoline(void) {
  uint64_t size = (uint64_t)(tramp_end - tramp_begin),
           gdt_off = (uint64_t)(tramp_gdt - tramp_begin),
           base_off = (uint64_t)(tramp_gdtr_base - tramp_begin),
           idtr_off = (uint64_t)(tramp_idtr_base - tramp_begin),
           idt_off = (uint64_t)(tramp_idt - tramp_begin),
           stub_off = (uint64_t)(tramp_stub_table - tramp_begin);
  uint32_t cs_index = uefi_cs >> 3;
  if (!size || size > 3 * PAGE || base_off + 8 > size || idtr_off + 8 > size ||
      idt_off + 4096 > size || stub_off + 256 * 16 > size || !cs_index ||
      cs_index >= 32 || cs_index == 2 || (uefi_cs & 7))
    fail("TRAMPOLINE");
  uint8_t *dst = (void *)(uintptr_t)trampoline_phys;
  __builtin_memcpy(dst, tramp_begin, size);
  uint64_t *gdt = (uint64_t *)(dst + gdt_off);
  for (unsigned i = 0; i < 32; i++)
    gdt[i] = 0;
  gdt[2] = UINT64_C(0x00cf92000000ffff);
  gdt[cs_index] = UINT64_C(0x00af9a000000ffff);
  *(uint64_t *)(dst + base_off) = trampoline_phys + gdt_off;
  *(uint64_t *)(dst + idtr_off) = trampoline_phys + idt_off;
  for (uint32_t i = 0; i < 256; i++) {
    uint64_t h = trampoline_phys + stub_off + i * 16;
    uint8_t *gate = dst + idt_off + i * 16;
    *(uint16_t *)(gate) = (uint16_t)h;
    *(uint16_t *)(gate + 2) = uefi_cs;
    gate[4] = 0;
    gate[5] = 0x8e;
    *(uint16_t *)(gate + 6) = (uint16_t)(h >> 16);
    *(uint32_t *)(gate + 8) = (uint32_t)(h >> 32);
    *(uint32_t *)(gate + 12) = 0;
  }
}
static uint64_t build_info(uint64_t map_size, uint64_t stride, uint32_t version,
                           uint64_t *total) {
  uint64_t needed = 0;
  if (stride > UINT32_MAX ||
      lbp_decode_uefi(map_buffer, map_size, (uint32_t)stride, version, raw,
                      MAX_RAW, &needed) != LBP_OK ||
      needed > MAX_RAW)
    fail_quiet();
  uint64_t count = 0;
  if (lbp_normalize(raw, needed, reservations, reservation_count, scratch,
                    MAX_MAP, norm, MAX_MAP, &count) != LBP_OK)
    fail_quiet();
  for (uint64_t i = 0; i < count; i++)
    if (norm[i].base < 0x100000 && norm[i].type == LBP_USABLE)
      norm[i].type = LBP_RESERVED;
  uint8_t *blob = (uint8_t *)(uintptr_t)info_phys, *p = blob + 64;
  uint32_t len = record(p, 1, 32 + (uint32_t)count * 32, 32, (uint32_t)count);
  __builtin_memcpy(p + len, norm, count * 32);
  p += 32 + count * 32;
  len = record(p, 2, 40 + kernel_count * 32, 40, kernel_count);
  __builtin_memcpy(p + len, kernel_segments, kernel_count * 32);
  __builtin_memcpy(p + 16, &kernel_entry, 8);
  p += 40 + kernel_count * 32;
  len = record(p, 3, 32 + reservation_count * 32, 32, reservation_count);
  __builtin_memcpy(p + len, reservations, reservation_count * 32);
  p += 32 + reservation_count * 32;
  len = record(p, 4, 72 + map_count * 32, 72, map_count);
  __builtin_memcpy(p + len, maps, map_count * 32);
  __builtin_memcpy(p + 16, &root_phys, 8);
  __builtin_memcpy(p + 24, &stack_phys, 8);
  uint64_t stack_size = 65536, rsp = stack_phys + 65536 - 8,
           guard = stack_phys - PAGE;
  __builtin_memcpy(p + 32, &stack_size, 8);
  __builtin_memcpy(p + 40, &rsp, 8);
  __builtin_memcpy(p + 48, &guard, 8);
  p += 72 + map_count * 32;
  __builtin_memcpy(p, &boot_device, sizeof boot_device);
  p += sizeof boot_device;
  if (have_fb) {
    framebuffer.record.flags = 0;
    __builtin_memcpy(p, &framebuffer, sizeof framebuffer);
    p += sizeof framebuffer;
  }
  if (have_acpi) {
    acpi_record.record.flags = 0;
    __builtin_memcpy(p, &acpi_record, sizeof acpi_record);
    p += sizeof acpi_record;
  }
  uint64_t provided = nx_enabled ? LBP_FEATURE_NX : 0;
  if (have_fb)
    provided |= LBP_FEATURE_FRAMEBUFFER;
  if (have_acpi)
    provided |= LBP_FEATURE_ACPI;
  lbp_header h = {{'L', 'A', 'M', 'I', 'X', 'L', 'B', 'P'},
                  1,
                  0,
                  64,
                  (uint64_t)(p - blob),
                  kernel_required_features,
                  provided,
                  64,
                  (uint32_t)(5 + have_fb + have_acpi),
                  0,
                  info_phys};
  __builtin_memcpy(blob, &h, 64);
  lbp_view view;
  lbp_status validation = lbp_validate(
      blob, h.total_size,
      have_acpi
          ? (lbp_span[]){{acpi_record.physical, acpi_bytes, acpi_record.length}}
          : 0,
      have_acpi ? 1 : 0, &view);
  if (validation != LBP_OK)
    fail_quiet();
  *total = h.total_size;
  return h.total_size;
}

void __attribute__((ms_abi)) efi_main(EFI_HANDLE handle, EFI_SYSTEM_TABLE *st) {
  image = handle;
  bs = st->boot;
  __asm__ volatile("mov %%cs,%0" : "=r"(uefi_cs));
  serial_init();
  serial("M06:EFI\n");
  uint32_t a, b, c, d;
  __asm__ volatile("cpuid"
                   : "=a"(a), "=b"(b), "=c"(c), "=d"(d)
                   : "a"(0x80000001), "c"(0));
  nx_enabled = !!(d & (1u << 20));
  uint64_t cr4;
  __asm__ volatile("mov %%cr4,%0" : "=r"(cr4));
  if (cr4 & (1u << 12))
    fail("LA57");
  if (nx_enabled) {
    uint32_t lo, hi;
    __asm__ volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(0xc0000080));
    lo |= 1u << 11;
    __asm__ volatile("wrmsr" ::"a"(lo), "d"(hi), "c"(0xc0000080) : "memory");
  }
  read_kernel(st);
  serial("M06:FILE\n");
  discover_fb(st);
  known_acpi(st);
  serial("M06:FEATURES\n");
  lamix_reader file = {0, file_read, file_size};
  uint64_t features = (nx_enabled ? LBP_FEATURE_NX : 0) |
                      (have_fb ? LBP_FEATURE_FRAMEBUFFER : 0) |
                      (have_acpi ? LBP_FEATURE_ACPI : 0);
  lamix_elf_plan plan;
  if (lamix_elf_inspect(file, features, &plan) != LBP_OK)
    fail("ELF");
  kernel_required_features = plan.required_features;
  serial("M06:ELF_OK\n");
  if (lamix_elf_load(file, &plan, elf_allocate, 0, kernel_segments, 32) !=
      LBP_OK)
    fail("ELF_LOAD");
  kernel_entry = plan.entry;
  kernel_count = plan.count;
  serial("M06:LOAD_OK\n");
  info_phys = alloc_pages(1048576, LBP_PURPOSE_BOOTINFO);
  uint64_t guarded = alloc_pages(65536 + PAGE, LBP_PURPOSE_STACK);
  stack_phys = guarded + PAGE;
  trampoline_phys = alloc_pages(3 * PAGE, LBP_PURPOSE_LOADER);
  prepare_trampoline();
  uint64_t mapbuf = alloc_pages(1024 * 1024, LBP_PURPOSE_LOADER);
  map_buffer = (void *)(uintptr_t)mapbuf;
  map_capacity = 1024 * 1024;
  make_page_tables();
  serial("M06:TABLES_OK\n");
  uint64_t map_size = map_capacity, key = 0, info_size = 0, stride = 0;
  uint32_t version = 0, exit_attempt = 0;
  int grew = 0;
  for (;;) {
    map_size = map_capacity;
    EFI_STATUS s =
        bs->get_memory_map(&map_size, map_buffer, &key, &stride, &version);
    if (s == EFI_BUFFER_TOO_SMALL) {
      uint64_t wanted = 0;
      if (!lamix_efi_grown_capacity(map_capacity, map_size, stride,
                                    (uint32_t)grew, &wanted))
        fail("MEMORY_MAP_CAPACITY");
      map_buffer = (void *)(uintptr_t)alloc_pages(wanted, LBP_PURPOSE_LOADER);
      map_capacity = wanted;
      grew = 1;
      continue;
    }
    if (EFI_ERROR(s) || stride < 40 || version != 1)
      fail("GET_MEMORY_MAP");
    build_info(map_size, stride, version, &info_size);
    s = bs->exit_boot_services(image, key);
    if (s == EFI_SUCCESS) {
      efi_transition(root_phys, stack_phys + 65536 - 8, info_phys, kernel_entry,
                     trampoline_phys);
    }
    if (!lamix_efi_retry_exit(s, exit_attempt))
      fail_quiet();
    exit_attempt++;
  }
}
