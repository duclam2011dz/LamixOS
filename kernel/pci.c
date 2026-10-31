/* SPDX-License-Identifier: GPL-3.0-only */
#include "pci.h"

static uint16_t read16(const uint8_t *p) {
  return (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
}
static uint32_t read32(const uint8_t *p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
         ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static uint64_t read64(const uint8_t *p) {
  return (uint64_t)read32(p) | ((uint64_t)read32(p + 4) << 32);
}

pci_status pci_parse_mcfg(const void *table_pointer, size_t size,
                          pci_mcfg_region *regions, size_t capacity,
                          size_t *count) {
  if (!table_pointer || !count || (!regions && capacity))
    return PCI_INVALID_ARGUMENT;
  const uint8_t *table = (const uint8_t *)table_pointer;
  if (size < 44 || size > UINT32_MAX || table[0] != 'M' || table[1] != 'C' ||
      table[2] != 'F' || table[3] != 'G' || read32(table + 4) != size)
    return PCI_BAD_CONFIG;
  size_t entries = (size - 44) / 16;
  if ((size - 44) % 16 || entries > capacity)
    return entries > capacity ? PCI_BUFFER_TOO_SMALL : PCI_BAD_CONFIG;
  uint8_t checksum = 0;
  for (size_t i = 0; i < size; ++i)
    checksum = (uint8_t)(checksum + table[i]);
  if (checksum || table[8] != 1 || read32(table + 40) != 0)
    return PCI_BAD_CONFIG;
  pci_mcfg_region parsed[PCI_MAX_MCFG_REGIONS];
  if (entries > PCI_MAX_MCFG_REGIONS)
    return PCI_UNSUPPORTED;
  for (size_t i = 0; i < entries; ++i) {
    const uint8_t *entry = table + 44 + i * 16;
    uint64_t base = read64(entry);
    uint16_t segment = read16(entry + 8);
    uint8_t start = entry[10], end = entry[11];
    if (!base || (base & ((UINT64_C(1) << 20) - 1)) || start > end ||
        read32(entry + 12) != 0)
      return PCI_BAD_CONFIG;
    /* MCFG base is the ECAM address for bus 0, not start_bus. */
    uint64_t span = ((uint64_t)end + 1) << 20;
    if (base > UINT64_MAX - span)
      return PCI_BAD_CONFIG;
    parsed[i] = (pci_mcfg_region){base, segment, start, end};
    for (size_t j = 0; j < i; ++j) {
      const pci_mcfg_region *prior = &parsed[j];
      if (prior->segment == segment && start <= prior->end_bus &&
          prior->start_bus <= end)
        return PCI_BAD_CONFIG;
    }
  }
  for (size_t i = 0; i < entries; ++i)
    regions[i] = parsed[i];
  *count = entries;
  return PCI_OK;
}

static pci_status enumerate_buses(const pci_config_ops *ops, uint16_t segment,
                                  unsigned first_bus, unsigned last_bus,
                                  pci_function *functions, size_t capacity,
                                  size_t *found) {
  for (unsigned bus = first_bus; bus <= last_bus; ++bus) {
    for (unsigned device = 0; device < 32; ++device) {
      pci_bdf bdf = {segment, (uint8_t)bus, (uint8_t)device, 0};
      uint32_t id = 0;
      if (!ops->read32(ops->context, bdf, 0, &id))
        return PCI_IO_ERROR;
      if ((id & 0xffffu) == 0xffffu)
        continue;
      uint32_t header = 0;
      if (!ops->read32(ops->context, bdf, 12, &header))
        return PCI_IO_ERROR;
      unsigned function_count = (header & UINT32_C(0x00800000)) ? 8 : 1;
      for (unsigned function = 0; function < function_count; ++function) {
        bdf.function = (uint8_t)function;
        if (function && !ops->read32(ops->context, bdf, 0, &id))
          return PCI_IO_ERROR;
        if ((id & 0xffffu) == 0xffffu)
          continue;
        uint32_t class_revision = 0, command_status = 0, header_type = 0;
        if (!ops->read32(ops->context, bdf, 8, &class_revision) ||
            !ops->read32(ops->context, bdf, 4, &command_status) ||
            !ops->read32(ops->context, bdf, 12, &header_type))
          return PCI_IO_ERROR;
        if (*found == capacity)
          return PCI_BUFFER_TOO_SMALL;
        pci_function item = {0};
        item.bdf = bdf;
        item.vendor_id = (uint16_t)id;
        item.device_id = (uint16_t)(id >> 16);
        item.command = (uint16_t)command_status;
        item.revision = (uint8_t)class_revision;
        item.programming_interface = (uint8_t)(class_revision >> 8);
        item.subclass = (uint8_t)(class_revision >> 16);
        item.class_code = (uint8_t)(class_revision >> 24);
        item.header_type = (uint8_t)(header_type >> 16);
        functions[(*found)++] = item;
      }
    }
  }
  return PCI_OK;
}

pci_status pci_enumerate(const pci_config_ops *ops, pci_function *functions,
                         size_t capacity, size_t *count) {
  if (!ops || !ops->read32 || !functions || !count)
    return PCI_INVALID_ARGUMENT;
  size_t found = 0;
  pci_status status = enumerate_buses(ops, 0, 0, 255, functions, capacity,
                                      &found);
  if (status != PCI_OK)
    return status;
  *count = found;
  return PCI_OK;
}

pci_status pci_enumerate_mcfg(const pci_config_ops *ops,
                              const pci_mcfg_region *regions,
                              size_t region_count, pci_function *functions,
                              size_t capacity, size_t *count) {
  if (!ops || !ops->read32 || (!regions && region_count) || !functions ||
      !count || region_count > PCI_MAX_MCFG_REGIONS)
    return PCI_INVALID_ARGUMENT;
  size_t found = 0;
  pci_status status = enumerate_buses(ops, 0, 0, 255, functions, capacity,
                                      &found);
  if (status != PCI_OK)
    return status;
  for (size_t i = 0; i < region_count; ++i) {
    const pci_mcfg_region *region = &regions[i];
    if (!region->base || region->start_bus > region->end_bus)
      return PCI_INVALID_ARGUMENT;
    if (region->segment == 0)
      continue;
    status = enumerate_buses(ops, region->segment, region->start_bus,
                             region->end_bus, functions, capacity, &found);
    if (status != PCI_OK)
      return status;
  }
  *count = found;
  return PCI_OK;
}

pci_status pci_probe_bars(const pci_config_ops *ops, pci_function *function) {
  if (!ops || !ops->read32 || !ops->write32 || !function ||
      (function->header_type & 0x7f) > 1)
    return PCI_INVALID_ARGUMENT;
  unsigned bars = (function->header_type & 0x7f) ? 2 : PCI_BAR_COUNT;
  uint32_t command_status = 0;
  if (!ops->read32(ops->context, function->bdf, 4, &command_status))
    return PCI_IO_ERROR;
  uint32_t disabled = (command_status & UINT32_C(0xffff)) & ~UINT32_C(7);
  if (!ops->write32(ops->context, function->bdf, 4,
                    disabled))
    return PCI_IO_ERROR;
  pci_bar parsed[PCI_BAR_COUNT] = {{0}};
  pci_status result = PCI_OK;
  for (unsigned i = 0; i < bars; ++i) {
    uint16_t offset = (uint16_t)(0x10 + i * 4);
    uint32_t original = 0, original_high = 0, mask = 0, mask_high = 0;
    if (!ops->read32(ops->context, function->bdf, offset, &original)) {
      result = PCI_IO_ERROR;
      break;
    }
    int is_io = original & 1u;
    unsigned type = is_io ? 0 : (original >> 1) & 3u;
    int is_64 = !is_io && type == 2;
    if (is_64 && (i + 1 >= bars ||
                  !ops->read32(ops->context, function->bdf, offset + 4,
                               &original_high))) {
      result = PCI_BAD_CONFIG;
      break;
    }
    if (!ops->write32(ops->context, function->bdf, offset, UINT32_MAX)) {
      result = PCI_IO_ERROR;
      break;
    }
    if (is_64 && !ops->write32(ops->context, function->bdf, offset + 4,
                                UINT32_MAX)) {
      (void)ops->write32(ops->context, function->bdf, offset, original);
      result = PCI_IO_ERROR;
      break;
    }
    int mask_read = ops->read32(ops->context, function->bdf, offset, &mask);
    int high_mask_read = !is_64 ||
        ops->read32(ops->context, function->bdf, offset + 4, &mask_high);
    int low_restored = ops->write32(ops->context, function->bdf, offset,
                                    original);
    int high_restored = !is_64 ||
        ops->write32(ops->context, function->bdf, offset + 4, original_high);
    if (!mask_read || !high_mask_read || !low_restored || !high_restored) {
      result = PCI_IO_ERROR;
      break;
    }
    pci_bar bar = {.index = (uint8_t)i};
    if (is_io) {
      uint32_t address_mask = mask & ~UINT32_C(3);
      bar.is_io = 1;
      bar.base = original & ~UINT32_C(3);
      bar.size = address_mask ? (uint32_t)(~(uint32_t)address_mask + 1u) : 0;
    } else {
      bar.is_64 = (uint8_t)is_64;
      bar.prefetchable = (original >> 3) & 1u;
      uint64_t original_address = original & ~UINT32_C(15);
      uint64_t address_mask = mask & ~UINT32_C(15);
      if (is_64) {
        original_address |= (uint64_t)original_high << 32;
        address_mask |= (uint64_t)mask_high << 32;
      } else if (type == 3) {
        result = PCI_BAD_CONFIG;
        break;
      } else if (type == 1) {
        result = PCI_UNSUPPORTED;
        break;
      }
      bar.base = original_address;
      bar.size = address_mask
                     ? (bar.is_64 ? ~address_mask + 1u
                                  : (uint32_t)(~(uint32_t)address_mask + 1u))
                     : 0;
    }
    if (bar.size && (bar.size & (bar.size - 1))) {
      result = PCI_BAD_CONFIG;
      break;
    }
    if (bar.size && bar.base && (bar.base & (bar.size - 1))) {
      result = PCI_BAD_CONFIG;
      break;
    }
    bar.valid = bar.size != 0;
    parsed[i] = bar;
    if (bar.is_64)
      ++i;
  }
  if (!ops->write32(ops->context, function->bdf, 4,
                    (command_status & UINT32_C(0xffff))))
    return PCI_IO_ERROR;
  if (result == PCI_OK)
    for (unsigned i = 0; i < PCI_BAR_COUNT; ++i)
      function->bars[i] = parsed[i];
  return result;
}

pci_status pci_map_bar(const pci_mmio_ops *ops, const pci_function *function,
                       unsigned bar_index, uint64_t bar_offset,
                       uint64_t virtual_address, uint64_t length) {
  if (!ops || !ops->map_uncached || !function || bar_index >= PCI_BAR_COUNT ||
      !length || (virtual_address & UINT64_C(4095)) ||
      (length & UINT64_C(4095)) || (bar_offset & UINT64_C(4095)))
    return PCI_INVALID_ARGUMENT;
  const pci_bar *bar = &function->bars[bar_index];
  if (!bar->valid || bar->is_io || bar_offset > bar->size ||
      length > bar->size - bar_offset || bar->base > UINT64_MAX - bar_offset)
    return PCI_BAD_CONFIG;
  uint64_t physical = bar->base + bar_offset;
  if (physical >= (UINT64_C(1) << 52) ||
      length > (UINT64_C(1) << 52) - physical ||
      virtual_address > UINT64_MAX - length)
    return PCI_BAD_CONFIG;
  return ops->map_uncached(ops->context, virtual_address, physical, length)
             ? PCI_OK
             : PCI_IO_ERROR;
}

pci_status pci_unmap_bar(const pci_mmio_ops *ops, uint64_t virtual_address,
                         uint64_t length) {
  if (!ops || !ops->unmap || !length ||
      (virtual_address & UINT64_C(4095)) || (length & UINT64_C(4095)) ||
      virtual_address > UINT64_MAX - length)
    return PCI_INVALID_ARGUMENT;
  return ops->unmap(ops->context, virtual_address, length) ? PCI_OK
                                                          : PCI_IO_ERROR;
}

/* Platform implementation is linked by the freestanding kernel only. */
#ifdef LAMIX_KERNEL
static uint32_t cf8_address(pci_bdf bdf, uint16_t offset) {
  return UINT32_C(0x80000000) | ((uint32_t)bdf.bus << 16) |
         ((uint32_t)bdf.device << 11) | ((uint32_t)bdf.function << 8) |
         (offset & UINT16_C(0xfc));
}
static int cf8_read(void *context, pci_bdf bdf, uint16_t offset,
                    uint32_t *value) {
  (void)context;
  if (!value || bdf.segment || bdf.device >= 32 || bdf.function >= 8 ||
      offset > 252 || (offset & 3))
    return 0;
  uint32_t address = cf8_address(bdf, offset);
  __asm__ volatile("outl %0,%%dx" : : "a"(address), "d"(UINT16_C(0x0cf8)) : "memory");
  __asm__ volatile("inl %%dx,%0" : "=a"(*value) : "d"(UINT16_C(0x0cfc)) : "memory");
  return 1;
}
static int cf8_write(void *context, pci_bdf bdf, uint16_t offset,
                     uint32_t value) {
  (void)context;
  if (bdf.segment || bdf.device >= 32 || bdf.function >= 8 || offset > 252 ||
      (offset & 3))
    return 0;
  uint32_t address = cf8_address(bdf, offset);
  __asm__ volatile("outl %0,%%dx" : : "a"(address), "d"(UINT16_C(0x0cf8)) : "memory");
  __asm__ volatile("outl %0,%%dx" : : "a"(value), "d"(UINT16_C(0x0cfc)) : "memory");
  return 1;
}
void pci_cf8_ops(pci_config_ops *ops) {
  if (ops)
    *ops = (pci_config_ops){0, cf8_read, cf8_write};
}
#else
void pci_cf8_ops(pci_config_ops *ops) {
  if (ops)
    *ops = (pci_config_ops){0};
}
#endif

static int ecam_contains(const pci_ecam_ops *ops, pci_bdf bdf) {
  if (!ops || !ops->regions)
    return 0;
  for (size_t i = 0; i < ops->region_count; ++i)
    if (ops->regions[i].segment == bdf.segment &&
        bdf.bus >= ops->regions[i].start_bus &&
        bdf.bus <= ops->regions[i].end_bus)
      return 1;
  return 0;
}

static int hybrid_read(void *context, pci_bdf bdf, uint16_t offset,
                       uint32_t *value) {
  pci_hybrid_ops *ops = (pci_hybrid_ops *)context;
  if (ecam_contains(&ops->ecam, bdf))
    return pci_ecam_access(&ops->ecam, bdf, offset, value, 0, 0);
  return ops->legacy.read32(ops->legacy.context, bdf, offset, value);
}

static int hybrid_write(void *context, pci_bdf bdf, uint16_t offset,
                        uint32_t value) {
  pci_hybrid_ops *ops = (pci_hybrid_ops *)context;
  if (ecam_contains(&ops->ecam, bdf))
    return pci_ecam_access(&ops->ecam, bdf, offset, 0, 1, value);
  return ops->legacy.write32(ops->legacy.context, bdf, offset, value);
}

void pci_hybrid_config_ops(pci_hybrid_ops *context, pci_config_ops *ops) {
  if (!context || !ops)
    return;
  *ops = (pci_config_ops){context, hybrid_read, hybrid_write};
}

int pci_ecam_access(void *context, pci_bdf bdf, uint16_t offset,
                    uint32_t *value, int write, uint32_t write_value) {
  pci_ecam_ops *ops = (pci_ecam_ops *)context;
  if (!ops || !ops->regions || !ops->map_dword || bdf.device >= 32 ||
      bdf.function >= 8 || offset > 4092 || (offset & 3) ||
      (!write && !value))
    return 0;
  for (size_t i = 0; i < ops->region_count; ++i) {
    const pci_mcfg_region *region = &ops->regions[i];
    if (region->segment != bdf.segment || bdf.bus < region->start_bus ||
        bdf.bus > region->end_bus)
      continue;
    uint64_t bus_offset = (uint64_t)bdf.bus << 20;
    uint64_t device_offset = (uint64_t)bdf.device << 15;
    uint64_t function_offset = (uint64_t)bdf.function << 12;
    uint64_t physical = region->base + bus_offset + device_offset +
                        function_offset + offset;
    if (physical < region->base)
      return 0;
    volatile uint32_t *word = ops->map_dword(ops->context, physical);
    if (!word)
      return 0;
    if (write)
      *word = write_value;
    else
      *value = *word;
    return 1;
  }
  return 0;
}

int pci_mcfg_access(void *context, pci_bdf bdf, uint16_t offset,
                    uint32_t *value, int write, uint32_t write_value) {
  return pci_ecam_access(context, bdf, offset, value, write, write_value);
}
