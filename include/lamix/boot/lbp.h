/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef LAMIX_BOOT_LBP_H
#define LAMIX_BOOT_LBP_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
#define LBP_ASSERT static_assert
#define LBP_ALIGNOF alignof
#else
#define LBP_ASSERT _Static_assert
#define LBP_ALIGNOF _Alignof
#endif
#define LBP_MAGIC "LAMIXLBP"
#define LBP_NOTE_MAGIC "LAMIXREQ"
#define LBP_ENTRY_MAGIC UINT64_C(0x50424c58494d414c)
#define LBP_MAJOR 1u
#define LBP_MINOR 0u
#define LBP_MAX_SIZE UINT64_C(1048576)
#define LBP_MAX_RECORDS 256u
#define LBP_REQUIRED 1u
#define LBP_FEATURE_FRAMEBUFFER UINT64_C(1)
#define LBP_FEATURE_ACPI UINT64_C(2)
#define LBP_FEATURE_NX UINT64_C(4)
#define LBP_FEATURES UINT64_C(7)
#define LBP_MEMORY_MAP 1u
#define LBP_KERNEL 2u
#define LBP_RESERVATIONS 3u
#define LBP_ENTRY 4u
#define LBP_BOOT_DEVICE 5u
#define LBP_MODULES 6u
#define LBP_FRAMEBUFFER 7u
#define LBP_ACPI 8u
#define LBP_USABLE 1u
#define LBP_RESERVED 2u
#define LBP_BAD 3u
#define LBP_ACPI_RECLAIM 4u
#define LBP_ACPI_NVS 5u
#define LBP_LOADER_RECLAIM 6u
#define LBP_RUNTIME 7u
#define LBP_MEMORY_UEFI 1u
#define LBP_MEMORY_NONVOLATILE 2u
#define LBP_READ 1u
#define LBP_WRITE 2u
#define LBP_EXECUTE 4u
#define LBP_PURPOSE_KERNEL 1u
#define LBP_PURPOSE_MODULE 2u
#define LBP_PURPOSE_BOOTINFO 3u
#define LBP_PURPOSE_STACK 4u
#define LBP_PURPOSE_TABLES 5u
#define LBP_PURPOSE_LOADER 6u
#define LBP_RELEASE_NEVER 0u
#define LBP_RELEASE_COPIED 1u
#define LBP_RELEASE_REPLACED 2u
#define LBP_SOURCE_BIOS 1u
#define LBP_SOURCE_UEFI 2u
#define LBP_SLOT_A 1u
#define LBP_SLOT_B 2u

typedef struct lbp_header {
  uint8_t magic[8];
  uint16_t major;
  uint16_t minor;
  uint32_t header_size;
  uint64_t total_size;
  uint64_t required_features;
  uint64_t provided_features;
  uint64_t records_offset;
  uint32_t record_count;
  uint32_t reserved;
  uint64_t self_phys;
} lbp_header;
LBP_ASSERT(sizeof(lbp_header) == 64, "sizeof lbp_header");
LBP_ASSERT(LBP_ALIGNOF(lbp_header) == 8, "align lbp_header");
LBP_ASSERT(offsetof(lbp_header, magic) == 0, "lbp_header.magic");
LBP_ASSERT(offsetof(lbp_header, major) == 8, "lbp_header.major");
LBP_ASSERT(offsetof(lbp_header, minor) == 10, "lbp_header.minor");
LBP_ASSERT(offsetof(lbp_header, header_size) == 12, "lbp_header.header_size");
LBP_ASSERT(offsetof(lbp_header, total_size) == 16, "lbp_header.total_size");
LBP_ASSERT(offsetof(lbp_header, required_features) == 24,
           "lbp_header.required_features");
LBP_ASSERT(offsetof(lbp_header, provided_features) == 32,
           "lbp_header.provided_features");
LBP_ASSERT(offsetof(lbp_header, records_offset) == 40,
           "lbp_header.records_offset");
LBP_ASSERT(offsetof(lbp_header, record_count) == 48, "lbp_header.record_count");
LBP_ASSERT(offsetof(lbp_header, reserved) == 52, "lbp_header.reserved");
LBP_ASSERT(offsetof(lbp_header, self_phys) == 56, "lbp_header.self_phys");

typedef struct lbp_record {
  uint32_t type;
  uint32_t flags;
  uint64_t size;
} lbp_record;
LBP_ASSERT(sizeof(lbp_record) == 16, "sizeof lbp_record");
LBP_ASSERT(LBP_ALIGNOF(lbp_record) == 8, "align lbp_record");
LBP_ASSERT(offsetof(lbp_record, type) == 0, "lbp_record.type");
LBP_ASSERT(offsetof(lbp_record, flags) == 4, "lbp_record.flags");
LBP_ASSERT(offsetof(lbp_record, size) == 8, "lbp_record.size");

typedef struct lbp_table {
  lbp_record record;
  uint32_t version;
  uint32_t stride;
  uint64_t count;
} lbp_table;
LBP_ASSERT(sizeof(lbp_table) == 32, "sizeof lbp_table");
LBP_ASSERT(LBP_ALIGNOF(lbp_table) == 8, "align lbp_table");
LBP_ASSERT(offsetof(lbp_table, record) == 0, "lbp_table.record");
LBP_ASSERT(offsetof(lbp_table, version) == 16, "lbp_table.version");
LBP_ASSERT(offsetof(lbp_table, stride) == 20, "lbp_table.stride");
LBP_ASSERT(offsetof(lbp_table, count) == 24, "lbp_table.count");

typedef struct lbp_memory {
  uint64_t base;
  uint64_t length;
  uint32_t type;
  uint32_t flags;
  uint64_t firmware_attributes;
} lbp_memory;
LBP_ASSERT(sizeof(lbp_memory) == 32, "sizeof lbp_memory");
LBP_ASSERT(LBP_ALIGNOF(lbp_memory) == 8, "align lbp_memory");
LBP_ASSERT(offsetof(lbp_memory, base) == 0, "lbp_memory.base");
LBP_ASSERT(offsetof(lbp_memory, length) == 8, "lbp_memory.length");
LBP_ASSERT(offsetof(lbp_memory, type) == 16, "lbp_memory.type");
LBP_ASSERT(offsetof(lbp_memory, flags) == 20, "lbp_memory.flags");
LBP_ASSERT(offsetof(lbp_memory, firmware_attributes) == 24,
           "lbp_memory.firmware_attributes");

typedef struct lbp_kernel {
  lbp_record record;
  uint64_t entry;
  uint32_t version;
  uint32_t stride;
  uint64_t count;
} lbp_kernel;
LBP_ASSERT(sizeof(lbp_kernel) == 40, "sizeof lbp_kernel");
LBP_ASSERT(LBP_ALIGNOF(lbp_kernel) == 8, "align lbp_kernel");
LBP_ASSERT(offsetof(lbp_kernel, record) == 0, "lbp_kernel.record");
LBP_ASSERT(offsetof(lbp_kernel, entry) == 16, "lbp_kernel.entry");
LBP_ASSERT(offsetof(lbp_kernel, version) == 24, "lbp_kernel.version");
LBP_ASSERT(offsetof(lbp_kernel, stride) == 28, "lbp_kernel.stride");
LBP_ASSERT(offsetof(lbp_kernel, count) == 32, "lbp_kernel.count");

typedef struct lbp_segment {
  uint64_t physical;
  uint64_t virtual_address;
  uint64_t length;
  uint32_t permissions;
  uint32_t reserved;
} lbp_segment;
LBP_ASSERT(sizeof(lbp_segment) == 32, "sizeof lbp_segment");
LBP_ASSERT(LBP_ALIGNOF(lbp_segment) == 8, "align lbp_segment");
LBP_ASSERT(offsetof(lbp_segment, physical) == 0, "lbp_segment.physical");
LBP_ASSERT(offsetof(lbp_segment, virtual_address) == 8,
           "lbp_segment.virtual_address");
LBP_ASSERT(offsetof(lbp_segment, length) == 16, "lbp_segment.length");
LBP_ASSERT(offsetof(lbp_segment, permissions) == 24, "lbp_segment.permissions");
LBP_ASSERT(offsetof(lbp_segment, reserved) == 28, "lbp_segment.reserved");

typedef struct lbp_reservation {
  uint64_t base;
  uint64_t length;
  uint32_t purpose;
  uint32_t release;
  uint64_t reserved;
} lbp_reservation;
LBP_ASSERT(sizeof(lbp_reservation) == 32, "sizeof lbp_reservation");
LBP_ASSERT(LBP_ALIGNOF(lbp_reservation) == 8, "align lbp_reservation");
LBP_ASSERT(offsetof(lbp_reservation, base) == 0, "lbp_reservation.base");
LBP_ASSERT(offsetof(lbp_reservation, length) == 8, "lbp_reservation.length");
LBP_ASSERT(offsetof(lbp_reservation, purpose) == 16, "lbp_reservation.purpose");
LBP_ASSERT(offsetof(lbp_reservation, release) == 20, "lbp_reservation.release");
LBP_ASSERT(offsetof(lbp_reservation, reserved) == 24,
           "lbp_reservation.reserved");

typedef struct lbp_entry {
  lbp_record record;
  uint64_t cr3;
  uint64_t stack_physical;
  uint64_t stack_size;
  uint64_t rsp;
  uint64_t guard;
  uint32_t version;
  uint32_t stride;
  uint64_t count;
} lbp_entry;
LBP_ASSERT(sizeof(lbp_entry) == 72, "sizeof lbp_entry");
LBP_ASSERT(LBP_ALIGNOF(lbp_entry) == 8, "align lbp_entry");
LBP_ASSERT(offsetof(lbp_entry, record) == 0, "lbp_entry.record");
LBP_ASSERT(offsetof(lbp_entry, cr3) == 16, "lbp_entry.cr3");
LBP_ASSERT(offsetof(lbp_entry, stack_physical) == 24,
           "lbp_entry.stack_physical");
LBP_ASSERT(offsetof(lbp_entry, stack_size) == 32, "lbp_entry.stack_size");
LBP_ASSERT(offsetof(lbp_entry, rsp) == 40, "lbp_entry.rsp");
LBP_ASSERT(offsetof(lbp_entry, guard) == 48, "lbp_entry.guard");
LBP_ASSERT(offsetof(lbp_entry, version) == 56, "lbp_entry.version");
LBP_ASSERT(offsetof(lbp_entry, stride) == 60, "lbp_entry.stride");
LBP_ASSERT(offsetof(lbp_entry, count) == 64, "lbp_entry.count");

typedef struct lbp_device {
  lbp_record record;
  uint32_t source;
  uint32_t block_size;
  uint8_t disk_guid[16];
  uint8_t esp_guid[16];
  uint8_t system_guid[16];
  uint32_t slot;
  uint32_t reserved;
} lbp_device;
LBP_ASSERT(sizeof(lbp_device) == 80, "sizeof lbp_device");
LBP_ASSERT(LBP_ALIGNOF(lbp_device) == 8, "align lbp_device");
LBP_ASSERT(offsetof(lbp_device, record) == 0, "lbp_device.record");
LBP_ASSERT(offsetof(lbp_device, source) == 16, "lbp_device.source");
LBP_ASSERT(offsetof(lbp_device, block_size) == 20, "lbp_device.block_size");
LBP_ASSERT(offsetof(lbp_device, disk_guid) == 24, "lbp_device.disk_guid");
LBP_ASSERT(offsetof(lbp_device, esp_guid) == 40, "lbp_device.esp_guid");
LBP_ASSERT(offsetof(lbp_device, system_guid) == 56, "lbp_device.system_guid");
LBP_ASSERT(offsetof(lbp_device, slot) == 72, "lbp_device.slot");
LBP_ASSERT(offsetof(lbp_device, reserved) == 76, "lbp_device.reserved");

typedef struct lbp_module {
  uint64_t physical;
  uint64_t length;
  uint64_t name_offset;
  uint32_t name_length;
  uint32_t reserved;
} lbp_module;
LBP_ASSERT(sizeof(lbp_module) == 32, "sizeof lbp_module");
LBP_ASSERT(LBP_ALIGNOF(lbp_module) == 8, "align lbp_module");
LBP_ASSERT(offsetof(lbp_module, physical) == 0, "lbp_module.physical");
LBP_ASSERT(offsetof(lbp_module, length) == 8, "lbp_module.length");
LBP_ASSERT(offsetof(lbp_module, name_offset) == 16, "lbp_module.name_offset");
LBP_ASSERT(offsetof(lbp_module, name_length) == 24, "lbp_module.name_length");
LBP_ASSERT(offsetof(lbp_module, reserved) == 28, "lbp_module.reserved");

typedef struct lbp_framebuffer {
  lbp_record record;
  uint64_t physical;
  uint64_t length;
  uint32_t width;
  uint32_t height;
  uint32_t pitch;
  uint32_t bpp;
  uint32_t red_mask;
  uint32_t green_mask;
  uint32_t blue_mask;
  uint32_t reserved_mask;
} lbp_framebuffer;
LBP_ASSERT(sizeof(lbp_framebuffer) == 64, "sizeof lbp_framebuffer");
LBP_ASSERT(LBP_ALIGNOF(lbp_framebuffer) == 8, "align lbp_framebuffer");
LBP_ASSERT(offsetof(lbp_framebuffer, record) == 0, "lbp_framebuffer.record");
LBP_ASSERT(offsetof(lbp_framebuffer, physical) == 16,
           "lbp_framebuffer.physical");
LBP_ASSERT(offsetof(lbp_framebuffer, length) == 24, "lbp_framebuffer.length");
LBP_ASSERT(offsetof(lbp_framebuffer, width) == 32, "lbp_framebuffer.width");
LBP_ASSERT(offsetof(lbp_framebuffer, height) == 36, "lbp_framebuffer.height");
LBP_ASSERT(offsetof(lbp_framebuffer, pitch) == 40, "lbp_framebuffer.pitch");
LBP_ASSERT(offsetof(lbp_framebuffer, bpp) == 44, "lbp_framebuffer.bpp");
LBP_ASSERT(offsetof(lbp_framebuffer, red_mask) == 48,
           "lbp_framebuffer.red_mask");
LBP_ASSERT(offsetof(lbp_framebuffer, green_mask) == 52,
           "lbp_framebuffer.green_mask");
LBP_ASSERT(offsetof(lbp_framebuffer, blue_mask) == 56,
           "lbp_framebuffer.blue_mask");
LBP_ASSERT(offsetof(lbp_framebuffer, reserved_mask) == 60,
           "lbp_framebuffer.reserved_mask");

typedef struct lbp_acpi {
  lbp_record record;
  uint64_t physical;
  uint32_t length;
  uint32_t revision;
} lbp_acpi;
LBP_ASSERT(sizeof(lbp_acpi) == 32, "sizeof lbp_acpi");
LBP_ASSERT(LBP_ALIGNOF(lbp_acpi) == 8, "align lbp_acpi");
LBP_ASSERT(offsetof(lbp_acpi, record) == 0, "lbp_acpi.record");
LBP_ASSERT(offsetof(lbp_acpi, physical) == 16, "lbp_acpi.physical");
LBP_ASSERT(offsetof(lbp_acpi, length) == 24, "lbp_acpi.length");
LBP_ASSERT(offsetof(lbp_acpi, revision) == 28, "lbp_acpi.revision");

typedef struct lbp_note {
  uint8_t magic[8];
  uint16_t major;
  uint16_t minimum_minor;
  uint32_t reserved;
  uint64_t required_features;
  uint64_t optional_features;
} lbp_note;
LBP_ASSERT(sizeof(lbp_note) == 32, "sizeof lbp_note");
LBP_ASSERT(LBP_ALIGNOF(lbp_note) == 8, "align lbp_note");
LBP_ASSERT(offsetof(lbp_note, magic) == 0, "lbp_note.magic");
LBP_ASSERT(offsetof(lbp_note, major) == 8, "lbp_note.major");
LBP_ASSERT(offsetof(lbp_note, minimum_minor) == 10, "lbp_note.minimum_minor");
LBP_ASSERT(offsetof(lbp_note, reserved) == 12, "lbp_note.reserved");
LBP_ASSERT(offsetof(lbp_note, required_features) == 16,
           "lbp_note.required_features");
LBP_ASSERT(offsetof(lbp_note, optional_features) == 24,
           "lbp_note.optional_features");

#endif
