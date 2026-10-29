/* SPDX-License-Identifier: GPL-3.0-only */
#include "vmm.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#define FAKE_PAGES 128
#define PTE_PRESENT UINT64_C(1)
#define PTE_WRITE UINT64_C(2)
#define PTE_PWT UINT64_C(8)
#define PTE_PCD UINT64_C(16)
#define PTE_NX UINT64_C(0x8000000000000000)

typedef struct fake_page {
  uint64_t physical;
  uint64_t entries[512];
  int used;
} fake_page;

typedef struct fake_machine {
  fake_page pages[FAKE_PAGES];
  uint64_t next_physical;
  int allocation_budget;
  uint64_t invalidations;
} fake_machine;

static int fake_allocate(void *context, uint64_t *physical) {
  fake_machine *machine = context;
  if (machine->allocation_budget == 0)
    return 0;
  if (machine->allocation_budget > 0)
    --machine->allocation_budget;
  for (unsigned i = 0; i < FAKE_PAGES; ++i) {
    if (!machine->pages[i].used) {
      fake_page *page = &machine->pages[i];
      page->used = 1;
      page->physical = machine->next_physical;
      machine->next_physical += VMM_PAGE_SIZE;
      for (unsigned j = 0; j < 512; ++j)
        page->entries[j] = 0;
      *physical = page->physical;
      return 1;
    }
  }
  return 0;
}

static int fake_release(void *context, uint64_t physical) {
  fake_machine *machine = context;
  for (unsigned i = 0; i < FAKE_PAGES; ++i) {
    if (machine->pages[i].used && machine->pages[i].physical == physical) {
      for (unsigned j = 0; j < 512; ++j)
        assert(machine->pages[i].entries[j] == 0);
      machine->pages[i].used = 0;
      return 1;
    }
  }
  return 0;
}

static volatile uint64_t *fake_access(void *context, uint64_t physical) {
  fake_machine *machine = context;
  for (unsigned i = 0; i < FAKE_PAGES; ++i)
    if (machine->pages[i].used && machine->pages[i].physical == physical)
      return machine->pages[i].entries;
  return 0;
}

static void fake_invalidate(void *context, uint64_t virtual_address) {
  fake_machine *machine = context;
  assert((virtual_address & (VMM_PAGE_SIZE - 1)) == 0);
  ++machine->invalidations;
}

static unsigned used_pages(const fake_machine *machine) {
  unsigned count = 0;
  for (unsigned i = 0; i < FAKE_PAGES; ++i)
    count += machine->pages[i].used != 0;
  return count;
}

int main(void) {
  fake_machine machine = {.next_physical = UINT64_C(0x100000),
                          .allocation_budget = -1};
  vmm_operations operations = {.context = &machine,
                               .allocate_page = fake_allocate,
                               .release_page = fake_release,
                               .access_page = fake_access,
                               .invalidate = fake_invalidate};
  vmm_space space = {0};
  assert(vmm_init(&space, &operations, 1) == VMM_OK);
  assert(space.root_physical == UINT64_C(0x100000));
  unsigned root_only = used_pages(&machine);
  assert(root_only == 1);

  const uint64_t va = UINT64_C(0xffff800000400000);
  const uint64_t pa = UINT64_C(0x0000001200000000);
  uint64_t physical = 0, flags = 0;
  uint32_t permissions = 0;
  assert(vmm_map_range(&space, va + 1, pa, VMM_PAGE_SIZE,
                      VMM_READ | VMM_WRITE, 0) == VMM_INVALID_ARGUMENT);
  assert(vmm_map_range(&space, UINT64_C(0x0000800000000000), pa,
                       VMM_PAGE_SIZE, VMM_READ, 0) == VMM_INVALID_ARGUMENT);
  assert(vmm_map_range(&space, va, VMM_PHYSICAL_LIMIT, VMM_PAGE_SIZE,
                       VMM_READ, 0) == VMM_INVALID_ARGUMENT);
  assert(vmm_map_range(&space, va, pa, VMM_PAGE_SIZE,
                       VMM_READ | VMM_WRITE | VMM_EXECUTE, 0) ==
         VMM_INVALID_ARGUMENT);
  assert(vmm_map_range(&space, va, pa, 2 * VMM_PAGE_SIZE,
                       VMM_READ | VMM_WRITE, VMM_MAP_UNCACHED) == VMM_OK);
  assert(vmm_query(&space, va + 37, &physical, &permissions, &flags) == VMM_OK);
  assert(physical == pa + 37);
  assert(permissions == (VMM_READ | VMM_WRITE));
  assert((flags & (PTE_PRESENT | PTE_WRITE | PTE_PWT | PTE_PCD | PTE_NX)) ==
         (PTE_PRESENT | PTE_WRITE | PTE_PWT | PTE_PCD | PTE_NX));
  assert(vmm_map_range(&space, va, pa, VMM_PAGE_SIZE, VMM_READ, 0) ==
         VMM_ALREADY_MAPPED);
  assert(vmm_protect_range(&space, va, 3 * VMM_PAGE_SIZE, VMM_READ) ==
         VMM_NOT_MAPPED);
  assert(vmm_query(&space, va, &physical, &permissions, &flags) == VMM_OK);
  assert(permissions == (VMM_READ | VMM_WRITE));
  assert(vmm_protect_range(&space, va, 2 * VMM_PAGE_SIZE, VMM_READ) == VMM_OK);
  assert(vmm_query(&space, va, &physical, &permissions, &flags) == VMM_OK);
  assert(permissions == VMM_READ && (flags & PTE_NX));
  assert(vmm_unmap_range(&space, va, 3 * VMM_PAGE_SIZE) == VMM_NOT_MAPPED);
  assert(vmm_query(&space, va, &physical, &permissions, &flags) == VMM_OK);
  assert(vmm_unmap_range(&space, va, 2 * VMM_PAGE_SIZE) == VMM_OK);
  assert(vmm_query(&space, va, &physical, &permissions, &flags) ==
         VMM_NOT_MAPPED);
  assert(vmm_query(&space, va + VMM_PAGE_SIZE, &physical, &permissions,
                   &flags) == VMM_NOT_MAPPED);
  assert(used_pages(&machine) == root_only);

  /* OOM after the first page crosses a 2 MiB boundary; rollback removes the
   * first mapping and every newly-created empty table while retaining leaf
   * ownership with the caller. */
  const uint64_t crossing = UINT64_C(0xffff8000001ff000);
  machine.allocation_budget = 3;
  assert(vmm_map_range(&space, crossing, UINT64_C(0x8000000),
                       2 * VMM_PAGE_SIZE, VMM_READ | VMM_WRITE, 0) ==
         VMM_OUT_OF_MEMORY);
  machine.allocation_budget = -1;
  assert(used_pages(&machine) == root_only);
  assert(vmm_query(&space, crossing, &physical, &permissions, &flags) ==
         VMM_NOT_MAPPED);
  assert(vmm_query(&space, crossing + VMM_PAGE_SIZE, &physical, &permissions,
                   &flags) == VMM_NOT_MAPPED);

  assert(vmm_map_range(&space, va, pa, VMM_PAGE_SIZE, VMM_READ, 0) == VMM_OK);
  uint64_t scratch_pt = 0;
  assert(vmm_prepare_scratch(&space, UINT64_C(0xffff900000000000),
                             UINT64_C(0xffff900000001000), &scratch_pt) ==
         VMM_OK);
  assert(scratch_pt != 0);
  assert(vmm_query(&space, UINT64_C(0xffff900000000000), &physical,
                   &permissions, &flags) == VMM_OK);
  assert(physical == space.root_physical);
  assert(vmm_query(&space, UINT64_C(0xffff900000001000), &physical,
                   &permissions, &flags) == VMM_OK);
  assert(physical == scratch_pt);
  assert(machine.invalidations >= 8);
  assert(vmm_destroy(&space) == VMM_OK);
  assert(used_pages(&machine) == 0);

  /* Without NX, non-executable requests stay readable but no NX bit is set;
   * W+X remains rejected independently of CPU support. */
  machine.allocation_budget = -1;
  assert(vmm_init(&space, &operations, 0) == VMM_OK);
  assert(vmm_map_range(&space, va, pa, VMM_PAGE_SIZE, VMM_READ, 0) == VMM_OK);
  assert(vmm_query(&space, va, &physical, &permissions, &flags) == VMM_OK);
  assert(!(flags & PTE_NX));
  assert(vmm_protect_range(&space, va, VMM_PAGE_SIZE,
                           VMM_READ | VMM_WRITE | VMM_EXECUTE) ==
         VMM_INVALID_ARGUMENT);
  assert(vmm_destroy(&space) == VMM_OK);
  assert(used_pages(&machine) == 0);
  puts("vmm native checks PASS");
  return 0;
}
