/* SPDX-License-Identifier: GPL-3.0-only */
#include "acpi.h"

static uint32_t read32(const uint8_t *p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
         ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint64_t read64(const uint8_t *p) {
  return (uint64_t)read32(p) | ((uint64_t)read32(p + 4) << 32);
}

static int checksum(const uint8_t *bytes, size_t size) {
  uint8_t sum = 0;
  for (size_t i = 0; i < size; ++i)
    sum = (uint8_t)(sum + bytes[i]);
  return sum == 0;
}

static int signature(const uint8_t *bytes, const char *wanted) {
  for (size_t i = 0; i < 4; ++i)
    if (bytes[i] != (uint8_t)wanted[i])
      return 0;
  return 1;
}

static acpi_status read_table(acpi_read_physical read_physical,
                              void *context, uint64_t physical,
                              void *scratch, size_t scratch_size,
                              uint32_t *length) {
  uint8_t header[ACPI_TABLE_HEADER_SIZE];
  /* Một số firmware QEMU đặt SDT ở địa chỉ byte-lệch; đọc/copy theo byte,
   * sau đó vẫn bắt buộc kiểm tra signature, chiều dài và checksum đầy đủ. */
  if (!physical || !read_physical(context, physical, header, sizeof(header)))
    return ACPI_BAD_ADDRESS;
  uint32_t size = read32(header + 4);
  if (size < ACPI_TABLE_HEADER_SIZE || size > ACPI_MAX_TABLE_SIZE)
    return ACPI_BAD_LENGTH;
  if (size > scratch_size)
    return ACPI_BUFFER_TOO_SMALL;
  if (!read_physical(context, physical, scratch, size))
    return ACPI_IO_ERROR;
  if (read32((const uint8_t *)scratch + 4) != size)
    return ACPI_BAD_LENGTH;
  if (!checksum(scratch, size))
    return ACPI_BAD_CHECKSUM;
  *length = size;
  return ACPI_OK;
}

acpi_status acpi_discover(const void *rsdp_pointer, size_t rsdp_size,
                          acpi_read_physical read_physical, void *context,
                          void *scratch, size_t scratch_size,
                          acpi_platform *platform) {
  if (!rsdp_pointer || !read_physical || !scratch || !platform)
    return ACPI_INVALID_ARGUMENT;
  const uint8_t *rsdp = (const uint8_t *)rsdp_pointer;
  if (rsdp_size < ACPI_RSDP_V1_SIZE)
    return ACPI_BAD_LENGTH;
  static const uint8_t rsdp_signature[8] = {'R','S','D',' ','P','T','R',' '};
  for (size_t i = 0; i < sizeof(rsdp_signature); ++i)
    if (rsdp[i] != rsdp_signature[i])
      return ACPI_BAD_SIGNATURE;
  if (!checksum(rsdp, ACPI_RSDP_V1_SIZE))
    return ACPI_BAD_CHECKSUM;

  uint8_t revision = rsdp[15];
  uint64_t root_physical = read32(rsdp + 16);
  uint8_t entry_size = 4;
  int xsdt = 0;
  if (revision == 0) {
    if (rsdp_size != ACPI_RSDP_V1_SIZE)
      return ACPI_BAD_LENGTH;
  } else if (revision == 2) {
    if (rsdp_size != ACPI_RSDP_V2_SIZE ||
        read32(rsdp + 20) != ACPI_RSDP_V2_SIZE)
      return ACPI_BAD_LENGTH;
    if (!checksum(rsdp, rsdp_size))
      return ACPI_BAD_CHECKSUM;
    uint64_t xsdt_physical = read64(rsdp + 24);
    if (xsdt_physical) {
      root_physical = xsdt_physical;
      xsdt = 1;
      entry_size = 8;
    }
  } else {
    return ACPI_UNSUPPORTED;
  }

  uint32_t root_length = 0;
  acpi_status status = read_table(read_physical, context, root_physical,
                                  scratch, scratch_size, &root_length);
  if (status != ACPI_OK)
    return status;
  const uint8_t *root = (const uint8_t *)scratch;
  if (!signature(root, xsdt ? "XSDT" : "RSDT"))
    return ACPI_BAD_SIGNATURE;
  if ((root_length - ACPI_TABLE_HEADER_SIZE) % entry_size)
    return ACPI_BAD_LENGTH;

  acpi_platform found = {0};
  found.root.physical = root_physical;
  found.root.length = root_length;
  found.root.entry_size = entry_size;
  found.root.xsdt = (uint8_t)xsdt;
  found.rsdp_revision = revision;
  uint32_t count = (root_length - ACPI_TABLE_HEADER_SIZE) / entry_size;
  for (uint32_t i = 0; i < count; ++i) {
    uint8_t entry[8] = {0};
    uint64_t entry_physical = root_physical + ACPI_TABLE_HEADER_SIZE +
                              (uint64_t)i * entry_size;
    if (entry_physical < root_physical ||
        !read_physical(context, entry_physical, entry, entry_size))
      return ACPI_IO_ERROR;
    uint64_t table_physical = xsdt ? read64(entry) : read32(entry);
    uint8_t header[ACPI_TABLE_HEADER_SIZE];
    if (!table_physical ||
        !read_physical(context, table_physical, header, sizeof(header)))
      return ACPI_BAD_ADDRESS;
    uint32_t table_length = read32(header + 4);
    if (table_length < ACPI_TABLE_HEADER_SIZE ||
        table_length > ACPI_MAX_TABLE_SIZE)
      return ACPI_BAD_LENGTH;
    if (signature(header, "APIC")) {
      if (found.madt_physical)
        return ACPI_BAD_ENTRY;
      uint32_t verified_length = 0;
      status = read_table(read_physical, context, table_physical, scratch,
                          scratch_size, &verified_length);
      if (status != ACPI_OK)
        return status;
      if (verified_length < 44)
        return ACPI_BAD_LENGTH;
      found.madt_physical = table_physical;
      found.madt_length = verified_length;
    } else if (signature(header, "MCFG")) {
      if (found.mcfg_physical)
        return ACPI_BAD_ENTRY;
      uint32_t verified_length = 0;
      status = read_table(read_physical, context, table_physical, scratch,
                          scratch_size, &verified_length);
      if (status != ACPI_OK)
        return status;
      if (verified_length < 44 || (verified_length - 44) % 16)
        return ACPI_BAD_LENGTH;
      found.mcfg_physical = table_physical;
      found.mcfg_length = verified_length;
    }
  }
  *platform = found;
  return ACPI_OK;
}

acpi_status acpi_madt_entry_at(const void *madt_pointer, size_t madt_size,
                               size_t *cursor, acpi_madt_entry *entry) {
  if (!madt_pointer || !cursor || !entry)
    return ACPI_INVALID_ARGUMENT;
  const uint8_t *madt = (const uint8_t *)madt_pointer;
  if (madt_size < 44 || !signature(madt, "APIC") ||
      read32(madt + 4) != madt_size)
    return ACPI_BAD_LENGTH;
  if (*cursor < 44 || *cursor > madt_size)
    return ACPI_INVALID_ARGUMENT;
  if (*cursor == madt_size)
    return ACPI_UNSUPPORTED;
  if (madt_size - *cursor < 2)
    return ACPI_BAD_ENTRY;
  uint8_t length = madt[*cursor + 1];
  if (length < 2 || length > madt_size - *cursor)
    return ACPI_BAD_ENTRY;
  static const uint8_t minimum_length[] = {8, 12, 10, 8, 6, 12, 0, 0, 0, 16,
                                            12, 24, 24, 16};
  uint8_t type = madt[*cursor];
  if (type < sizeof(minimum_length) && minimum_length[type] &&
      length < minimum_length[type])
    return ACPI_BAD_ENTRY;
  *entry = (acpi_madt_entry){type, length, madt + *cursor};
  *cursor += length;
  return ACPI_OK;
}

acpi_status acpi_madt_parse(const void *madt_pointer, size_t madt_size,
                            uint8_t bsp_apic_id, acpi_madt_info *info) {
  if (!madt_pointer || !info || madt_size < 44)
    return ACPI_INVALID_ARGUMENT;
  const uint8_t *madt = madt_pointer;
  if (!signature(madt, "APIC") || read32(madt + 4) != madt_size ||
      !checksum(madt, madt_size))
    return ACPI_BAD_CHECKSUM;
  acpi_madt_info parsed = {0};
  parsed.lapic_physical = read32(madt + 36);
  parsed.bsp_apic_id = bsp_apic_id;
  if (!parsed.lapic_physical || (parsed.lapic_physical & 0xfff))
    return ACPI_BAD_ADDRESS;
  size_t cursor = 44;
  for (;;) {
    acpi_madt_entry entry;
    acpi_status status = acpi_madt_entry_at(madt, madt_size, &cursor, &entry);
    if (status == ACPI_UNSUPPORTED)
      break;
    if (status != ACPI_OK)
      return status;
    const uint8_t *b = entry.bytes;
    if (entry.type == 0) {
      if (entry.length != 8) return ACPI_BAD_ENTRY;
      uint32_t flags = read32(b + 4);
      if ((flags & 3u) && b[3] == bsp_apic_id) {
        if (parsed.bsp_present) return ACPI_BAD_ENTRY;
        parsed.bsp_present = 1;
      }
      if (flags & 1u) {
        if (parsed.cpu_count == ACPI_MADT_MAX_CPUS)
          return ACPI_BUFFER_TOO_SMALL;
        for (size_t i = 0; i < parsed.cpu_count; ++i)
          if (parsed.cpus[i].apic_id == b[3] ||
              parsed.cpus[i].processor_id == b[2])
            return ACPI_BAD_ENTRY;
        parsed.cpus[parsed.cpu_count++] =
            (acpi_cpu_info){b[2], b[3], flags};
      }
    } else if (entry.type == 1) {
      if (entry.length != 12 || parsed.ioapic_count == ACPI_MADT_MAX_IOAPICS)
        return ACPI_BAD_ENTRY;
      acpi_ioapic_info io = {b[2], read32(b + 4), read32(b + 8)};
      if (!io.physical || (io.physical & 0xfff)) return ACPI_BAD_ADDRESS;
      for (size_t i = 0; i < parsed.ioapic_count; ++i)
        if (parsed.ioapics[i].id == io.id ||
            parsed.ioapics[i].gsi_base == io.gsi_base)
          return ACPI_BAD_ENTRY;
      parsed.ioapics[parsed.ioapic_count++] = io;
    } else if (entry.type == 2) {
      if (entry.length != 10 || parsed.override_count == ACPI_MADT_MAX_OVERRIDES)
        return ACPI_BAD_ENTRY;
      acpi_irq_override irq = {b[3], read32(b + 4),
                               (uint16_t)(b[8] | ((uint16_t)b[9] << 8))};
      if (irq.source >= 16 || ((irq.flags & 3u) == 2u) ||
          (((irq.flags >> 2) & 3u) == 2u) || (irq.flags & 0xfff0u))
        return ACPI_BAD_ENTRY;
      for (size_t i = 0; i < parsed.override_count; ++i)
        if (parsed.overrides[i].source == irq.source)
          return ACPI_BAD_ENTRY;
      parsed.overrides[parsed.override_count++] = irq;
    } else if (entry.type == 5) {
      if (entry.length != 12) return ACPI_BAD_ENTRY;
      uint64_t address = read64(b + 4);
      if (!address || (address & 0xfff)) return ACPI_BAD_ADDRESS;
      parsed.lapic_physical = address;
    } else if (entry.type == 9) {
      if (entry.length != 16) return ACPI_BAD_ENTRY;
      if (read32(b + 8) & 3u) parsed.x2apic_present = 1;
    }
  }
  if (!parsed.ioapic_count || !parsed.bsp_present) return ACPI_BAD_ENTRY;
  *info = parsed;
  return ACPI_OK;
}
