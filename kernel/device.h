/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef LAMIX_KERNEL_DEVICE_H
#define LAMIX_KERNEL_DEVICE_H

#include "pci.h"
#include <stddef.h>
#include <stdint.h>

#define DEVICE_REGISTRY_CAPACITY 256u
#define DEVICE_DMA_OWNER_NONE UINT32_C(0)

typedef enum device_status {
  DEVICE_OK,
  DEVICE_INVALID_ARGUMENT,
  DEVICE_FULL,
  DEVICE_NOT_FOUND,
  DEVICE_RESOURCE_BUSY,
  DEVICE_BAD_STATE
} device_status;

typedef struct device_dma_ownership {
  uintptr_t owner;
  uint32_t generation;
  uint8_t active;
} device_dma_ownership;

typedef struct device_record {
  pci_function pci;
  uintptr_t driver;
  uint32_t generation;
  uint32_t dma_generation;
  device_dma_ownership dma;
  uint8_t registered;
} device_record;

typedef struct device_registry {
  device_record *records;
  size_t capacity;
  size_t count;
  uint32_t next_generation;
} device_registry;

device_status device_registry_init(device_registry *registry,
                                  device_record *storage, size_t capacity);
device_status device_register_pci(device_registry *registry,
                                  const pci_function *function,
                                  size_t *index);
device_status device_get(device_registry *registry, size_t index,
                         device_record **record);
device_status device_bind_driver(device_record *record, uintptr_t driver);
device_status device_dma_acquire(device_record *record, uintptr_t owner,
                                 uint32_t *generation);
device_status device_dma_release(device_record *record, uintptr_t owner,
                                 uint32_t generation);

#endif
