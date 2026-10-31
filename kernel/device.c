/* SPDX-License-Identifier: GPL-3.0-only */
#include "device.h"

device_status device_registry_init(device_registry *registry,
                                  device_record *storage, size_t capacity) {
  if (!registry || !storage || !capacity)
    return DEVICE_INVALID_ARGUMENT;
  for (size_t i = 0; i < capacity; ++i)
    storage[i] = (device_record){0};
  *registry = (device_registry){storage, capacity, 0, 1};
  return DEVICE_OK;
}

device_status device_register_pci(device_registry *registry,
                                  const pci_function *function,
                                  size_t *index) {
  if (!registry || !registry->records || !function || !index)
    return DEVICE_INVALID_ARGUMENT;
  for (size_t i = 0; i < registry->count; ++i) {
    if (registry->records[i].registered &&
        registry->records[i].pci.bdf.segment == function->bdf.segment &&
        registry->records[i].pci.bdf.bus == function->bdf.bus &&
        registry->records[i].pci.bdf.device == function->bdf.device &&
        registry->records[i].pci.bdf.function == function->bdf.function)
      return DEVICE_BAD_STATE;
  }
  if (registry->count == registry->capacity)
    return DEVICE_FULL;
  size_t at = registry->count++;
  registry->records[at] = (device_record){.pci = *function,
      .generation = registry->next_generation++, .registered = 1};
  if (!registry->next_generation)
    registry->next_generation = 1;
  *index = at;
  return DEVICE_OK;
}

device_status device_get(device_registry *registry, size_t index,
                         device_record **record) {
  if (!registry || !record || !registry->records)
    return DEVICE_INVALID_ARGUMENT;
  if (index >= registry->count || !registry->records[index].registered)
    return DEVICE_NOT_FOUND;
  *record = &registry->records[index];
  return DEVICE_OK;
}

device_status device_bind_driver(device_record *record, uintptr_t driver) {
  if (!record || !record->registered || !driver)
    return DEVICE_INVALID_ARGUMENT;
  if (record->driver)
    return DEVICE_BAD_STATE;
  record->driver = driver;
  return DEVICE_OK;
}

device_status device_dma_acquire(device_record *record, uintptr_t owner,
                                 uint32_t *generation) {
  if (!record || !record->registered || !owner || !generation)
    return DEVICE_INVALID_ARGUMENT;
  if (record->dma.active)
    return DEVICE_RESOURCE_BUSY;
  ++record->dma_generation;
  if (!record->dma_generation)
    ++record->dma_generation;
  record->dma.owner = owner;
  record->dma.generation = record->dma_generation;
  record->dma.active = 1;
  *generation = record->dma.generation;
  return DEVICE_OK;
}

device_status device_dma_release(device_record *record, uintptr_t owner,
                                 uint32_t generation) {
  if (!record || !record->registered || !owner)
    return DEVICE_INVALID_ARGUMENT;
  if (!record->dma.active || record->dma.owner != owner ||
      record->dma.generation != generation)
    return DEVICE_BAD_STATE;
  record->dma.owner = DEVICE_DMA_OWNER_NONE;
  record->dma.active = 0;
  return DEVICE_OK;
}
