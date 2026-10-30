/* SPDX-License-Identifier: GPL-3.0-only */
#include "vmm.h"

#define PTE_PRESENT UINT64_C(0x001)
#define PTE_WRITE UINT64_C(0x002)
#define PTE_PWT UINT64_C(0x008)
#define PTE_PCD UINT64_C(0x010)
#define PTE_HUGE UINT64_C(0x080)
#define PTE_NX UINT64_C(0x8000000000000000)
#define PTE_ADDRESS UINT64_C(0x000ffffffffff000)
#define PTE_PERMISSION_MASK (PTE_WRITE | PTE_NX)
#define PTE_ATTRIBUTE_MASK (PTE_PWT | PTE_PCD)
#define PTE_ALLOWED_MASK (PTE_PRESENT | PTE_PERMISSION_MASK | PTE_ATTRIBUTE_MASK | PTE_ADDRESS)
#define VMM_MAGIC UINT32_C(0x564d4d31)

typedef struct vmm_path {
  uint64_t tables[4];
  uint16_t indices[4];
  uint8_t depth;
} vmm_path;

static int canonical(uint64_t address) {
  uint64_t upper = address >> 48;
  return ((address >> 47) & 1) ? upper == UINT64_C(0xffff)
                               : upper == 0;
}

static int valid_permissions(uint32_t permissions) {
  const uint32_t allowed = VMM_READ | VMM_WRITE | VMM_EXECUTE;
  return (permissions & VMM_READ) && !(permissions & ~allowed) &&
         (permissions & (VMM_WRITE | VMM_EXECUTE)) !=
             (VMM_WRITE | VMM_EXECUTE);
}

static int valid_range(uint64_t virtual_address, uint64_t physical_address,
                       uint64_t length) {
  if (!length || (virtual_address & (VMM_PAGE_SIZE - 1)) ||
      (physical_address & (VMM_PAGE_SIZE - 1)) ||
      (length & (VMM_PAGE_SIZE - 1)) || virtual_address > UINT64_MAX - length ||
      physical_address > UINT64_MAX - length)
    return 0;
  uint64_t virtual_end = virtual_address + length - 1;
  uint64_t physical_end = physical_address + length;
  return canonical(virtual_address) && canonical(virtual_end) &&
         ((virtual_address >> 47) == (virtual_end >> 47)) &&
         physical_end <= VMM_PHYSICAL_LIMIT;
}

static int valid_space(const vmm_space *space) {
  return space && space->initialized == VMM_MAGIC &&
         space->operations.allocate_page && space->operations.release_page &&
         space->operations.access_page && space->operations.invalidate;
}

static vmm_status access_table(vmm_space *space, uint64_t physical,
                               volatile uint64_t **table) {
  *table = space->operations.access_page(space->operations.context, physical);
  return *table ? VMM_OK : VMM_CALLBACK_FAILED;
}

static vmm_status allocate_table(vmm_space *space, uint64_t *physical) {
  if (!space->operations.allocate_page(space->operations.context, physical))
    return VMM_OUT_OF_MEMORY;
  if ((*physical & (VMM_PAGE_SIZE - 1)) || *physical >= VMM_PHYSICAL_LIMIT) {
    (void)space->operations.release_page(space->operations.context, *physical);
    return VMM_CALLBACK_FAILED;
  }
  volatile uint64_t *table = 0;
  vmm_status status = access_table(space, *physical, &table);
  if (status != VMM_OK) {
    (void)space->operations.release_page(space->operations.context, *physical);
    return status;
  }
  for (unsigned i = 0; i < 512; ++i)
    table[i] = 0;
  return VMM_OK;
}

/* Access callbacks may remap one scratch VA. Re-fetch the parent after an
 * allocation/access callback before writing its entry. */
static vmm_status walk_leaf(vmm_space *space, uint64_t virtual_address,
                            int create, vmm_path *path,
                            volatile uint64_t **leaf) {
  uint64_t table_physical = space->root_physical;
  path->tables[0] = table_physical;
  path->depth = 1;
  for (unsigned level = 0; level < 3; ++level) {
    unsigned shift = 39 - level * 9;
    uint16_t index = (uint16_t)((virtual_address >> shift) & 511);
    path->indices[level] = index;
    volatile uint64_t *table = 0;
    vmm_status status = access_table(space, table_physical, &table);
    if (status != VMM_OK)
      return status;
    uint64_t entry = table[index];
    if (!(entry & PTE_PRESENT)) {
      if (!create)
        return VMM_NOT_MAPPED;
      uint64_t child = 0;
      status = allocate_table(space, &child);
      if (status != VMM_OK)
        return status;
      status = access_table(space, table_physical, &table);
      if (status != VMM_OK) {
        (void)space->operations.release_page(space->operations.context, child);
        return status;
      }
      if (table[index] & PTE_PRESENT) {
        (void)space->operations.release_page(space->operations.context, child);
        return VMM_CORRUPT_TABLE;
      }
      table[index] = child | PTE_PRESENT | PTE_WRITE;
      entry = table[index];
    } else if (entry & PTE_HUGE) {
      return VMM_CORRUPT_TABLE;
    }
    table_physical = entry & PTE_ADDRESS;
    path->tables[level + 1] = table_physical;
    path->depth = (uint8_t)(level + 2);
  }
  path->indices[3] = (uint16_t)((virtual_address >> 12) & 511);
  vmm_status status = access_table(space, table_physical, leaf);
  return status;
}

static uint64_t make_leaf(const vmm_space *space, uint64_t physical,
                          uint32_t permissions, uint32_t attributes) {
  uint64_t entry = physical | PTE_PRESENT;
  if (permissions & VMM_WRITE)
    entry |= PTE_WRITE;
  if (space->nx_enabled && !(permissions & VMM_EXECUTE))
    entry |= PTE_NX;
  if (attributes & VMM_MAP_UNCACHED)
    entry |= PTE_PWT | PTE_PCD;
  return entry;
}

static void invalidate(vmm_space *space, uint64_t virtual_address) {
  space->operations.invalidate(space->operations.context, virtual_address);
}

static int table_empty(vmm_space *space, uint64_t physical,
                       vmm_status *status) {
  volatile uint64_t *table = 0;
  *status = access_table(space, physical, &table);
  if (*status != VMM_OK)
    return 0;
  for (unsigned i = 0; i < 512; ++i)
    if (table[i] & PTE_PRESENT)
      return 0;
  return 1;
}

static vmm_status prune_path(vmm_space *space, uint64_t virtual_address) {
  vmm_path path = {0};
  volatile uint64_t *leaf = 0;
  vmm_status status = walk_leaf(space, virtual_address, 0, &path, &leaf);
  if (status != VMM_OK && status != VMM_NOT_MAPPED)
    return status;
  (void)leaf;
  int deepest = status == VMM_NOT_MAPPED ? (int)path.depth - 1 : 3;
  for (int level = deepest; level > 0; --level) {
    if (!table_empty(space, path.tables[level], &status))
      return status;
    volatile uint64_t *parent = 0;
    status = access_table(space, path.tables[level - 1], &parent);
    if (status != VMM_OK)
      return status;
    uint64_t entry = parent[path.indices[level - 1]];
    if (!(entry & PTE_PRESENT) || (entry & PTE_ADDRESS) != path.tables[level])
      return VMM_CORRUPT_TABLE;
    parent[path.indices[level - 1]] = 0;
    if (!space->operations.release_page(space->operations.context,
                                        path.tables[level]))
      return VMM_CALLBACK_FAILED;
  }
  return VMM_OK;
}

static vmm_status preflight(vmm_space *space, uint64_t virtual_address,
                            uint64_t length, int must_be_mapped) {
  for (uint64_t offset = 0; offset < length; offset += VMM_PAGE_SIZE) {
    vmm_path path = {0};
    volatile uint64_t *leaf = 0;
    vmm_status status = walk_leaf(space, virtual_address + offset, 0, &path,
                                  &leaf);
    if (status == VMM_NOT_MAPPED) {
      if (must_be_mapped)
        return VMM_NOT_MAPPED;
      continue;
    }
    if (status != VMM_OK)
      return status;
    int present = !!(leaf[path.indices[3]] & PTE_PRESENT);
    if (present && !must_be_mapped)
      return VMM_ALREADY_MAPPED;
    if (!present && must_be_mapped)
      return VMM_NOT_MAPPED;
  }
  return VMM_OK;
}

vmm_status vmm_init(vmm_space *space, const vmm_operations *operations,
                    int nx_enabled) {
  if (!space || !operations || !operations->allocate_page ||
      !operations->release_page || !operations->access_page ||
      !operations->invalidate)
    return VMM_INVALID_ARGUMENT;
  vmm_space candidate = {.operations = *operations,
                         .root_physical = 0,
                         .nx_enabled = nx_enabled != 0,
                         .initialized = 0};
  vmm_status status = allocate_table(&candidate, &candidate.root_physical);
  if (status != VMM_OK)
    return status;
  candidate.initialized = VMM_MAGIC;
  *space = candidate;
  return VMM_OK;
}

static vmm_status destroy_table(vmm_space *space, uint64_t physical,
                                unsigned level) {
  volatile uint64_t *table = 0;
  vmm_status status = access_table(space, physical, &table);
  if (status != VMM_OK)
    return status;
  uint64_t entries[512];
  for (unsigned i = 0; i < 512; ++i)
    entries[i] = table[i];
  if (level > 1) {
    for (unsigned i = 0; i < 512; ++i) {
      if (!(entries[i] & PTE_PRESENT))
        continue;
      if (entries[i] & PTE_HUGE)
        return VMM_CORRUPT_TABLE;
      status = destroy_table(space, entries[i] & PTE_ADDRESS, level - 1);
      if (status != VMM_OK)
        return status;
    }
  }
  status = access_table(space, physical, &table);
  if (status != VMM_OK)
    return status;
  for (unsigned i = 0; i < 512; ++i)
    table[i] = 0;
  if (!space->operations.release_page(space->operations.context, physical))
    return VMM_CALLBACK_FAILED;
  return VMM_OK;
}

vmm_status vmm_destroy(vmm_space *space) {
  if (!valid_space(space))
    return VMM_INVALID_ARGUMENT;
  vmm_status status = destroy_table(space, space->root_physical, 4);
  if (status == VMM_OK) {
    space->root_physical = 0;
    space->initialized = 0;
  }
  return status;
}

vmm_status vmm_map_range(vmm_space *space, uint64_t virtual_address,
                         uint64_t physical_address, uint64_t length,
                         uint32_t permissions, uint32_t attributes) {
  if (!valid_space(space) || !valid_range(virtual_address, physical_address,
                                         length) ||
      !valid_permissions(permissions) || (attributes & ~VMM_MAP_UNCACHED))
    return VMM_INVALID_ARGUMENT;
  vmm_status status = preflight(space, virtual_address, length, 0);
  if (status != VMM_OK)
    return status;
  uint64_t completed = 0;
  for (uint64_t offset = 0; offset < length; offset += VMM_PAGE_SIZE) {
    vmm_path path = {0};
    volatile uint64_t *leaf = 0;
    status = walk_leaf(space, virtual_address + offset, 1, &path, &leaf);
    if (status == VMM_OK && !(leaf[path.indices[3]] & PTE_PRESENT)) {
      leaf[path.indices[3]] = make_leaf(space, physical_address + offset,
                                        permissions, attributes);
      invalidate(space, virtual_address + offset);
      completed += VMM_PAGE_SIZE;
      continue;
    }
    if (status == VMM_OK)
      status = VMM_ALREADY_MAPPED;
    uint64_t rollback = 0;
    while (rollback < completed) {
      vmm_path prior = {0};
      volatile uint64_t *prior_leaf = 0;
      vmm_status rollback_status =
          walk_leaf(space, virtual_address + rollback, 0, &prior, &prior_leaf);
      if (rollback_status != VMM_OK)
        return VMM_CORRUPT_TABLE;
      prior_leaf[prior.indices[3]] = 0;
      invalidate(space, virtual_address + rollback);
      rollback += VMM_PAGE_SIZE;
    }
    for (uint64_t cleanup = 0; cleanup <= completed; cleanup += VMM_PAGE_SIZE) {
      vmm_status cleanup_status =
          prune_path(space, virtual_address + cleanup);
      if (cleanup_status != VMM_OK)
        return cleanup_status;
    }
    return status;
  }
  return VMM_OK;
}

vmm_status vmm_unmap_range(vmm_space *space, uint64_t virtual_address,
                           uint64_t length) {
  if (!valid_space(space) || !valid_range(virtual_address, 0, length))
    return VMM_INVALID_ARGUMENT;
  vmm_status status = preflight(space, virtual_address, length, 1);
  if (status != VMM_OK)
    return status;
  for (uint64_t offset = 0; offset < length; offset += VMM_PAGE_SIZE) {
    vmm_path path = {0};
    volatile uint64_t *leaf = 0;
    status = walk_leaf(space, virtual_address + offset, 0, &path, &leaf);
    if (status != VMM_OK)
      return status;
    leaf[path.indices[3]] = 0;
    invalidate(space, virtual_address + offset);
  }
  for (uint64_t offset = 0; offset < length; offset += VMM_PAGE_SIZE) {
    status = prune_path(space, virtual_address + offset);
    if (status != VMM_OK)
      return status;
  }
  return VMM_OK;
}

vmm_status vmm_protect_range(vmm_space *space, uint64_t virtual_address,
                             uint64_t length, uint32_t permissions) {
  if (!valid_space(space) || !valid_range(virtual_address, 0, length) ||
      !valid_permissions(permissions))
    return VMM_INVALID_ARGUMENT;
  vmm_status status = preflight(space, virtual_address, length, 1);
  if (status != VMM_OK)
    return status;
  for (uint64_t offset = 0; offset < length; offset += VMM_PAGE_SIZE) {
    vmm_path path = {0};
    volatile uint64_t *leaf = 0;
    status = walk_leaf(space, virtual_address + offset, 0, &path, &leaf);
    if (status != VMM_OK)
      return status;
    uint64_t entry = leaf[path.indices[3]] &
                     ~(PTE_WRITE | PTE_NX);
    if (permissions & VMM_WRITE)
      entry |= PTE_WRITE;
    if (space->nx_enabled && !(permissions & VMM_EXECUTE))
      entry |= PTE_NX;
    leaf[path.indices[3]] = entry;
    invalidate(space, virtual_address + offset);
  }
  return VMM_OK;
}

vmm_status vmm_query(vmm_space *space, uint64_t virtual_address,
                     uint64_t *physical_address, uint32_t *permissions,
                     uint64_t *entry_flags) {
  if (!valid_space(space) || !canonical(virtual_address) ||
      !physical_address || !permissions || !entry_flags)
    return VMM_INVALID_ARGUMENT;
  vmm_path path = {0};
  volatile uint64_t *leaf = 0;
  vmm_status status = walk_leaf(space, virtual_address, 0, &path, &leaf);
  if (status != VMM_OK)
    return status;
  uint64_t entry = leaf[path.indices[3]];
  if (!(entry & PTE_PRESENT))
    return VMM_NOT_MAPPED;
  *physical_address = (entry & PTE_ADDRESS) |
                      (virtual_address & (VMM_PAGE_SIZE - 1));
  *permissions = VMM_READ | ((entry & PTE_WRITE) ? VMM_WRITE : 0) |
                 ((!space->nx_enabled || !(entry & PTE_NX)) ? VMM_EXECUTE
                                                             : 0);
  *entry_flags = entry & ~PTE_ADDRESS;
  return VMM_OK;
}

vmm_status vmm_prepare_scratch(vmm_space *space, uint64_t data_virtual,
                               uint64_t control_virtual,
                               uint64_t *control_page_physical) {
  if (!valid_space(space) || !control_page_physical ||
      !canonical(data_virtual) || !canonical(control_virtual) ||
      (data_virtual & (VMM_PAGE_SIZE - 1)) ||
      (control_virtual & (VMM_PAGE_SIZE - 1)) ||
      ((data_virtual >> 21) != (control_virtual >> 21)) ||
      data_virtual == control_virtual)
    return VMM_INVALID_ARGUMENT;
  vmm_status status = vmm_map_range(space, data_virtual, space->root_physical,
                                    VMM_PAGE_SIZE, VMM_READ | VMM_WRITE, 0);
  if (status != VMM_OK)
    return status;
  vmm_path path = {0};
  volatile uint64_t *leaf = 0;
  status = walk_leaf(space, data_virtual, 0, &path, &leaf);
  if (status != VMM_OK)
    return status;
  uint64_t table_physical = path.tables[3];
  status = vmm_map_range(space, control_virtual, table_physical, VMM_PAGE_SIZE,
                         VMM_READ | VMM_WRITE, 0);
  if (status == VMM_OK)
    *control_page_physical = table_physical;
  return status;
}
