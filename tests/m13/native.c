/* SPDX-License-Identifier: GPL-3.0-only */
#include "acpi.h"
#include "device.h"
#include "pci.h"
#include <assert.h>
#include <stdint.h>
#include <string.h>

static uint8_t physical[0x5000];
static uint64_t mapped_address;
static void put32(uint8_t *p, uint32_t value) {
  p[0] = (uint8_t)value; p[1] = (uint8_t)(value >> 8);
  p[2] = (uint8_t)(value >> 16); p[3] = (uint8_t)(value >> 24);
}
static void put64(uint8_t *p, uint64_t value) {
  put32(p, (uint32_t)value); put32(p + 4, (uint32_t)(value >> 32));
}
static void set_sum(uint8_t *p, size_t length, size_t at) {
  uint8_t sum = 0; p[at] = 0;
  for (size_t i = 0; i < length; ++i) sum = (uint8_t)(sum + p[i]);
  p[at] = (uint8_t)(0u - sum);
}
static volatile uint32_t *map_test_dword(void *context, uint64_t address) {
  mapped_address = address;
  return (volatile uint32_t *)context;
}
static int phys_read(void *context, uint64_t address, void *out, size_t length) {
  (void)context;
  if (address > sizeof(physical) || length > sizeof(physical) - address)
    return 0;
  memcpy(out, physical + address, length);
  return 1;
}
static void table_header(uint8_t *table, const char *signature, uint32_t size) {
  memcpy(table, signature, 4); put32(table + 4, size); table[8] = 1;
  memcpy(table + 10, "LAMIX ", 6); memcpy(table + 16, "M13TEST ", 8);
  put32(table + 24, 1); memcpy(table + 28, "LMX1", 4); put32(table + 32, 1);
}

static void test_acpi(void) {
  uint8_t *rsdp = physical + 0x100;
  uint8_t *xsdt = physical + 0x1000;
  uint8_t *madt = physical + 0x2000;
  uint8_t *mcfg = physical + 0x3000;
  memcpy(rsdp, "RSD PTR ", 8); memcpy(rsdp + 9, "LAMIX ", 6);
  rsdp[15] = 2; put32(rsdp + 20, 36); put64(rsdp + 24, 0x1000);
  set_sum(rsdp, 20, 8); set_sum(rsdp, 36, 32);
  table_header(xsdt, "XSDT", 52); put64(xsdt + 36, 0x2000);
  put64(xsdt + 44, 0x3000); set_sum(xsdt, 52, 9);
  table_header(madt, "APIC", 44); put32(madt + 36, 0xfee00000);
  set_sum(madt, 44, 9);
  table_header(mcfg, "MCFG", 60); put64(mcfg + 44, 0xe0000000);
  mcfg[54] = 0; mcfg[55] = 255; set_sum(mcfg, 60, 9);
  uint8_t scratch[128]; acpi_platform platform = {0};
  assert(acpi_discover(rsdp, 36, phys_read, 0, scratch, sizeof(scratch),
                       &platform) == ACPI_OK);
  assert(platform.root.xsdt && platform.root.physical == 0x1000);
  assert(platform.madt_physical == 0x2000 && platform.madt_length == 44);
  assert(platform.mcfg_physical == 0x3000 && platform.mcfg_length == 60);
  size_t cursor = 44; acpi_madt_entry entry;
  uint8_t entry_madt[52] = {0}; table_header(entry_madt, "APIC", 52);
  put32(entry_madt + 36, 0xfee00000);
  entry_madt[44] = 0; entry_madt[45] = 8; set_sum(entry_madt, 52, 9);
  cursor = 44;
  assert(acpi_madt_entry_at(entry_madt, sizeof(entry_madt), &cursor, &entry) == ACPI_OK);
  assert(entry.type == 0 && entry.length == 8 && cursor == 52);
  entry_madt[45] = 1;
  cursor = 44;
  assert(acpi_madt_entry_at(entry_madt, sizeof(entry_madt), &cursor,
                            &entry) == ACPI_BAD_ENTRY);
  pci_mcfg_region regions[2]; size_t count = 0;
  assert(pci_parse_mcfg(mcfg, 60, regions, 2, &count) == PCI_OK);
  assert(count == 1 && regions[0].base == UINT64_C(0xe0000000) &&
         regions[0].end_bus == 255);
  uint32_t ecam_word = UINT32_C(0xfeedc0de);
  pci_ecam_ops ecam = {&ecam_word, regions, count, map_test_dword};
  uint32_t ecam_value = 0; pci_bdf ecam_bdf = {0, 3, 2, 1};
  assert(pci_ecam_access(&ecam, ecam_bdf, 0x40, &ecam_value, 0, 0));
  assert(ecam_value == UINT32_C(0xfeedc0de) &&
         mapped_address == UINT64_C(0xe0311040));
  uint8_t invalid_mcfg[60]; memcpy(invalid_mcfg, mcfg, sizeof(invalid_mcfg));
  invalid_mcfg[59] ^= 1;
  assert(pci_parse_mcfg(invalid_mcfg, sizeof(invalid_mcfg), regions, 2,
                        &count) == PCI_BAD_CONFIG);
  uint8_t bad[44]; memcpy(bad, madt, sizeof(bad));
  cursor = 44;
  assert(acpi_madt_entry_at(bad, sizeof(bad), &cursor, &entry) == ACPI_UNSUPPORTED);
  assert(acpi_discover(rsdp, 35, phys_read, 0, scratch, sizeof(scratch),
                       &platform) == ACPI_BAD_LENGTH);
  rsdp[32] ^= 1;
  assert(acpi_discover(rsdp, 36, phys_read, 0, scratch, sizeof(scratch),
                       &platform) == ACPI_BAD_CHECKSUM);
  rsdp[32] ^= 1;
  xsdt[51] ^= 1;
  assert(acpi_discover(rsdp, 36, phys_read, 0, scratch, sizeof(scratch),
                       &platform) == ACPI_BAD_CHECKSUM);
  xsdt[51] ^= 1;
  mcfg[54] = 250; mcfg[55] = 1; set_sum(mcfg, 60, 9);
  assert(pci_parse_mcfg(mcfg, 60, regions, 2, &count) == PCI_BAD_CONFIG);
}

typedef struct fake_pci {
  uint32_t regs[2][8][64];
  uint32_t masks[2][8][6];
  uint8_t probing[2][8][6];
  unsigned reads;
  uint8_t fail_read;
  uint32_t last_command_write;
} fake_pci;
static int fake_read(void *context, pci_bdf bdf, uint16_t offset,
                     uint32_t *value) {
  fake_pci *fake = context; ++fake->reads;
  if (fake->fail_read)
    return 0;
  if (bdf.bus || bdf.device > 1 || bdf.function >= 8 || offset > 252 ||
      (offset & 3)) { *value = UINT32_MAX; return 1; }
  unsigned index = bdf.device;
  unsigned function = bdf.function;
  unsigned dword = offset / 4;
  if (!offset && (fake->regs[index][function][0] & 0xffffu) == 0) {
    *value = UINT32_MAX;
    return 1;
  }
  if (offset >= 0x10 && offset < 0x28 &&
      fake->probing[index][function][(offset-0x10)/4])
    *value = fake->masks[index][function][(offset-0x10)/4];
  else
    *value = fake->regs[index][function][dword];
  return 1;
}
static int fake_write(void *context, pci_bdf bdf, uint16_t offset,
                      uint32_t value) {
  fake_pci *fake = context;
  if (bdf.bus || bdf.device > 1 || bdf.function >= 8 || offset > 252 ||
      (offset & 3))
    return 1;
  unsigned index = bdf.device;
  unsigned function = bdf.function;
  if (offset == 4)
    fake->last_command_write = value;
  if (offset >= 0x10 && offset < 0x28) {
    unsigned bar = (offset - 0x10) / 4;
    fake->probing[index][function][bar] = value == UINT32_MAX;
  }
  if (value != UINT32_MAX)
    fake->regs[index][function][offset / 4] = value;
  return 1;
}

static void test_pci_and_registry(void) {
  fake_pci fake = {0}; pci_config_ops ops = {&fake, fake_read, fake_write};
  fake.regs[0][0][0] = UINT32_C(0x12348086);
  fake.regs[0][0][1] = UINT32_C(0x00000006);
  fake.regs[0][0][2] = UINT32_C(0x06000001);
  fake.regs[0][0][3] = 0;
  fake.regs[1][0][0] = UINT32_C(0x100e8086);
  fake.regs[1][0][1] = UINT32_C(0xabcd0002);
  fake.regs[1][0][2] = UINT32_C(0x02000002);
  fake.regs[1][0][3] = UINT32_C(0x00800000);
  fake.regs[1][0][4] = UINT32_C(0x20200004);
  fake.regs[1][0][5] = UINT32_C(1);
  fake.regs[1][1][0] = UINT32_C(0x56788086);
  fake.regs[1][1][1] = UINT32_C(0x00000002);
  fake.regs[1][1][2] = UINT32_C(0x02000002);
  fake.masks[1][0][0] = UINT32_C(0xffe00004);
  fake.masks[1][0][1] = UINT32_MAX;
  pci_function functions[4]; size_t count = 0;
  assert(pci_enumerate(&ops, functions, 4, &count) == PCI_OK);
  assert(count == 3 && functions[1].vendor_id == 0x8086 &&
         functions[1].device_id == 0x100e && functions[1].class_code == 2);
  assert(functions[2].bdf.function == 1 && functions[2].device_id == 0x5678);
  assert(pci_enumerate(&ops, functions, 2, &count) == PCI_BUFFER_TOO_SMALL);
  assert(pci_probe_bars(&ops, &functions[1]) == PCI_OK);
  assert(functions[1].bars[0].valid &&
         functions[1].bars[0].base == UINT64_C(0x120200000) &&
         functions[1].bars[0].size == UINT64_C(0x200000) &&
         !functions[1].bars[0].is_io && functions[1].bars[0].is_64);
  assert(fake.last_command_write == 2);
  device_record storage[2]; device_registry registry;
  assert(device_registry_init(&registry, storage, 2) == DEVICE_OK);
  size_t slot = 0; device_record *record = 0;
  assert(device_register_pci(&registry, &functions[1], &slot) == DEVICE_OK);
  assert(device_get(&registry, slot, &record) == DEVICE_OK && record);
  assert(device_bind_driver(record, 1) == DEVICE_OK);
  uint32_t generation = 0;
  assert(device_dma_acquire(record, 7, &generation) == DEVICE_OK);
  assert(device_dma_acquire(record, 8, &generation) == DEVICE_RESOURCE_BUSY);
  assert(device_dma_release(record, 8, generation) == DEVICE_BAD_STATE);
  assert(device_dma_release(record, 7, generation) == DEVICE_OK);
  uint32_t old_generation = generation;
  assert(device_dma_acquire(record, 7, &generation) == DEVICE_OK);
  assert(generation != old_generation);
  assert(device_dma_release(record, 7, old_generation) == DEVICE_BAD_STATE);
  assert(device_dma_release(record, 7, generation) == DEVICE_OK);
  fake.fail_read = 1;
  assert(pci_enumerate(&ops, functions, 4, &count) == PCI_IO_ERROR);
}

int main(void) { test_acpi(); test_pci_and_registry(); return 0; }
