/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef LAMIX_KERNEL_PCI_H
#define LAMIX_KERNEL_PCI_H

#include <stddef.h>
#include <stdint.h>

#define PCI_MAX_FUNCTIONS 256u
#define PCI_BAR_COUNT 6u
#define PCI_MAX_MCFG_REGIONS 16u

typedef enum pci_status {
  PCI_OK,
  PCI_INVALID_ARGUMENT,
  PCI_IO_ERROR,
  PCI_BUFFER_TOO_SMALL,
  PCI_BAD_CONFIG,
  PCI_UNSUPPORTED
} pci_status;

typedef struct pci_bdf {
  uint16_t segment;
  uint8_t bus;
  uint8_t device;
  uint8_t function;
} pci_bdf;

typedef struct pci_config_ops {
  void *context;
  int (*read32)(void *context, pci_bdf bdf, uint16_t offset,
                uint32_t *value);
  int (*write32)(void *context, pci_bdf bdf, uint16_t offset,
                 uint32_t value);
} pci_config_ops;

typedef struct pci_bar {
  uint64_t base;
  uint64_t size;
  uint8_t index;
  uint8_t is_io;
  uint8_t is_64;
  uint8_t prefetchable;
  uint8_t valid;
} pci_bar;

typedef struct pci_function {
  pci_bdf bdf;
  uint16_t vendor_id;
  uint16_t device_id;
  uint16_t command;
  uint8_t class_code;
  uint8_t subclass;
  uint8_t programming_interface;
  uint8_t revision;
  uint8_t header_type;
  pci_bar bars[PCI_BAR_COUNT];
} pci_function;

typedef struct pci_mcfg_region {
  uint64_t base;
  uint16_t segment;
  uint8_t start_bus;
  uint8_t end_bus;
} pci_mcfg_region;

typedef struct pci_ecam_ops {
  void *context;
  const pci_mcfg_region *regions;
  size_t region_count;
  volatile uint32_t *(*map_dword)(void *context, uint64_t physical);
} pci_ecam_ops;

typedef struct pci_hybrid_ops {
  pci_config_ops legacy;
  pci_ecam_ops ecam;
} pci_hybrid_ops;

typedef struct pci_mmio_ops {
  void *context;
  int (*map_uncached)(void *context, uint64_t virtual_address,
                      uint64_t physical_address, uint64_t length);
  int (*unmap)(void *context, uint64_t virtual_address, uint64_t length);
} pci_mmio_ops;

typedef int (*pci_config_access)(void *context, pci_bdf bdf,
                                 uint16_t offset, uint32_t *value,
                                 int write, uint32_t write_value);

pci_status pci_parse_mcfg(const void *table, size_t size,
                          pci_mcfg_region *regions, size_t capacity,
                          size_t *count);
pci_status pci_enumerate(const pci_config_ops *ops, pci_function *functions,
                         size_t capacity, size_t *count);
pci_status pci_enumerate_mcfg(const pci_config_ops *ops,
                              const pci_mcfg_region *regions,
                              size_t region_count, pci_function *functions,
                              size_t capacity, size_t *count);
pci_status pci_probe_bars(const pci_config_ops *ops, pci_function *function);
pci_status pci_map_bar(const pci_mmio_ops *ops, const pci_function *function,
                       unsigned bar_index, uint64_t bar_offset,
                       uint64_t virtual_address, uint64_t length);
pci_status pci_unmap_bar(const pci_mmio_ops *ops, uint64_t virtual_address,
                         uint64_t length);
int pci_mcfg_access(void *context, pci_bdf bdf, uint16_t offset,
                    uint32_t *value, int write, uint32_t write_value);
int pci_ecam_access(void *context, pci_bdf bdf, uint16_t offset,
                    uint32_t *value, int write, uint32_t write_value);
void pci_cf8_ops(pci_config_ops *ops);
void pci_hybrid_config_ops(pci_hybrid_ops *context, pci_config_ops *ops);

#endif
