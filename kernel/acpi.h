/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef LAMIX_KERNEL_ACPI_H
#define LAMIX_KERNEL_ACPI_H

#include <stddef.h>
#include <stdint.h>

#define ACPI_RSDP_V1_SIZE 20u
#define ACPI_RSDP_V2_SIZE 36u
#define ACPI_TABLE_HEADER_SIZE 36u
#define ACPI_MAX_TABLE_SIZE (64u * 1024u)
#define ACPI_MADT_MAX_IOAPICS 8u
#define ACPI_MADT_MAX_OVERRIDES 16u

typedef enum acpi_status {
  ACPI_OK,
  ACPI_INVALID_ARGUMENT,
  ACPI_BAD_SIGNATURE,
  ACPI_BAD_CHECKSUM,
  ACPI_BAD_LENGTH,
  ACPI_BAD_ADDRESS,
  ACPI_UNSUPPORTED,
  ACPI_IO_ERROR,
  ACPI_BUFFER_TOO_SMALL,
  ACPI_BAD_ENTRY
} acpi_status;

typedef int (*acpi_read_physical)(void *context, uint64_t physical,
                                  void *destination, size_t length);

typedef struct acpi_root {
  uint64_t physical;
  uint32_t length;
  uint8_t entry_size;
  uint8_t xsdt;
} acpi_root;

typedef struct acpi_platform {
  acpi_root root;
  uint64_t madt_physical;
  uint32_t madt_length;
  uint64_t mcfg_physical;
  uint32_t mcfg_length;
  uint8_t rsdp_revision;
} acpi_platform;

typedef struct acpi_madt_entry {
  uint8_t type;
  uint8_t length;
  const uint8_t *bytes;
} acpi_madt_entry;

typedef struct acpi_ioapic_info {
  uint8_t id;
  uint32_t physical;
  uint32_t gsi_base;
} acpi_ioapic_info;

typedef struct acpi_irq_override {
  uint8_t source;
  uint32_t gsi;
  uint16_t flags;
} acpi_irq_override;

typedef struct acpi_madt_info {
  uint64_t lapic_physical;
  acpi_ioapic_info ioapics[ACPI_MADT_MAX_IOAPICS];
  acpi_irq_override overrides[ACPI_MADT_MAX_OVERRIDES];
  size_t ioapic_count;
  size_t override_count;
  uint8_t bsp_apic_id;
  uint8_t bsp_present;
} acpi_madt_info;

acpi_status acpi_discover(const void *rsdp, size_t rsdp_size,
                          acpi_read_physical read_physical, void *context,
                          void *scratch, size_t scratch_size,
                          acpi_platform *platform);
acpi_status acpi_madt_entry_at(const void *madt, size_t madt_size,
                               size_t *cursor, acpi_madt_entry *entry);
acpi_status acpi_madt_parse(const void *madt, size_t madt_size,
                            uint8_t bsp_apic_id, acpi_madt_info *info);

#endif
