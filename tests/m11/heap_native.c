/* SPDX-License-Identifier: GPL-3.0-only */
#include "heap.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define SIM_PAGES 16
#define SIM_BASE UINT64_C(0xffffe00000000000)

typedef struct simulator {
  _Alignas(4096) uint8_t arena[SIM_PAGES * KHEAP_PAGE_SIZE];
  uint8_t mapped[SIM_PAGES];
  int map_budget;
} simulator;

static int sim_map(void *context, uint64_t address, void **accessible_page) {
  simulator *sim = context;
  if (address < SIM_BASE || ((address - SIM_BASE) & (KHEAP_PAGE_SIZE - 1)))
    return 0;
  uint64_t page = (address - SIM_BASE) / KHEAP_PAGE_SIZE;
  if (page >= SIM_PAGES || sim->mapped[page] || sim->map_budget == 0)
    return 0;
  if (sim->map_budget > 0)
    --sim->map_budget;
  sim->mapped[page] = 1;
  memset(sim->arena + page * KHEAP_PAGE_SIZE, 0xa5, KHEAP_PAGE_SIZE);
  *accessible_page = sim->arena + page * KHEAP_PAGE_SIZE;
  return 1;
}

static int sim_release(void *context, uint64_t address) {
  simulator *sim = context;
  if (address < SIM_BASE || ((address - SIM_BASE) & (KHEAP_PAGE_SIZE - 1)))
    return 0;
  uint64_t page = (address - SIM_BASE) / KHEAP_PAGE_SIZE;
  if (page >= SIM_PAGES || !sim->mapped[page])
    return 0;
  sim->mapped[page] = 0;
  return 1;
}

static unsigned sim_mapped_pages(const simulator *sim) {
  unsigned count = 0;
  for (unsigned i = 0; i < SIM_PAGES; ++i)
    count += sim->mapped[i] != 0;
  return count;
}

static void check_zero(const uint8_t *bytes, size_t length) {
  for (size_t i = 0; i < length; ++i)
    assert(bytes[i] == 0);
}

int main(void) {
  simulator sim = {.map_budget = -1};
  kheap_operations operations = {.context = &sim,
                                 .map_page = sim_map,
                                 .release_page = sim_release};
  kheap heap = {0};
  assert(kheap_init(&heap, &operations, SIM_BASE,
                    SIM_PAGES * KHEAP_PAGE_SIZE) == KHEAP_OK);
  assert(kheap_alloc(&heap, 0) == NULL);
  assert(heap.last_error == KHEAP_INVALID_ARGUMENT);
  assert(kheap_calloc(&heap, 0, 8) == NULL);
  assert(kheap_calloc(&heap, SIZE_MAX, 2) == NULL);
  assert(heap.last_error == KHEAP_OUT_OF_MEMORY);

  uint8_t *small = kheap_alloc(&heap, 1);
  uint8_t *medium = kheap_alloc(&heap, 333);
  uint8_t *zeroed = kheap_calloc(&heap, 32, 8);
  assert(small && medium && zeroed);
  assert(((uintptr_t)small & 15u) == 0);
  assert(((uintptr_t)medium & 15u) == 0);
  assert(((uintptr_t)zeroed & 15u) == 0);
  assert(small[0] == 0xa5);
  check_zero(zeroed, 256);
  memset(medium, 0x5a, 333);

  kheap_stats stats;
  assert(kheap_get_stats(&heap, &stats) == KHEAP_OK);
  assert(stats.live_allocations == 3 && stats.live_bytes == 590);
  assert(stats.peak_live_bytes == 590 && stats.mapped_pages == 1);
  assert(kheap_free(&heap, NULL) == KHEAP_OK);
  assert(kheap_free(&heap, small) == KHEAP_OK);
  assert(kheap_free(&heap, small) == KHEAP_DOUBLE_FREE);
  assert(kheap_free(&heap, sim.arena + 16) == KHEAP_INVALID_POINTER);
  assert(kheap_free(&heap, medium) == KHEAP_OK);
  assert(kheap_free(&heap, zeroed) == KHEAP_OK);
  assert(kheap_get_stats(&heap, &stats) == KHEAP_OK);
  assert(stats.live_allocations == 0 && stats.live_bytes == 0);
  assert(stats.invalid_free_count == 1 && stats.double_free_count == 1);
  assert(stats.allocation_count == 3 && stats.free_count == 3);

  /* Coalesce across page-backed blocks and reuse the same virtual span. */
  uint8_t *first = kheap_alloc(&heap, 4000);
  uint8_t *second = kheap_alloc(&heap, 4000);
  assert(first && second && first < second);
  assert(kheap_free(&heap, first) == KHEAP_OK);
  assert(kheap_free(&heap, second) == KHEAP_OK);
  uint64_t before_reuse = heap.mapped_pages;
  uint8_t *combined = kheap_alloc(&heap, 8000);
  assert(combined == first && heap.mapped_pages == before_reuse);
  assert(kheap_free(&heap, combined) == KHEAP_OK);

  /* Repeated varied allocations exercise split/coalesce and reuse paths. */
  void *stress[128] = {0};
  for (unsigned round = 0; round < 8; ++round) {
    for (unsigned i = 0; i < 128; ++i) {
      size_t size = 1u + ((i * 97u + round * 53u) % 700u);
      stress[i] = kheap_alloc(&heap, size);
      assert(stress[i] != NULL);
      memset(stress[i], (int)(round + 1), size);
    }
    for (unsigned i = 0; i < 128; ++i)
      assert(kheap_free(&heap, stress[(i * 37u) % 128u]) == KHEAP_OK);
    kheap_stats stress_stats;
    assert(kheap_get_stats(&heap, &stress_stats) == KHEAP_OK);
    assert(stress_stats.live_allocations == 0 && stress_stats.live_bytes == 0);
  }

  /* A failed multi-page extension must release every page mapped in the try. */
  simulator failing = {.map_budget = 1};
  kheap_operations failing_operations = {.context = &failing,
                                         .map_page = sim_map,
                                         .release_page = sim_release};
  kheap failed_heap = {0};
  assert(kheap_init(&failed_heap, &failing_operations, SIM_BASE,
                    SIM_PAGES * KHEAP_PAGE_SIZE) == KHEAP_OK);
  assert(kheap_alloc(&failed_heap, 8192) == NULL);
  assert(failed_heap.last_error == KHEAP_OUT_OF_MEMORY);
  assert(failed_heap.mapped_pages == 0 && sim_mapped_pages(&failing) == 0);
  assert(failed_heap.free_head == UINT64_MAX &&
         failed_heap.free_block_bytes == 0);
  failing.map_budget = -1;
  void *after_rollback = kheap_alloc(&failed_heap, 8192);
  assert(after_rollback && failed_heap.mapped_pages == 3);
  assert(kheap_free(&failed_heap, after_rollback) == KHEAP_OK);

  /* Host window exhaustion is deterministic and never changes live blocks. */
  simulator bounded = {.map_budget = -1};
  kheap_operations bounded_operations = {.context = &bounded,
                                         .map_page = sim_map,
                                         .release_page = sim_release};
  kheap bounded_heap = {0};
  assert(kheap_init(&bounded_heap, &bounded_operations, SIM_BASE,
                    KHEAP_PAGE_SIZE) == KHEAP_OK);
  void *too_large = kheap_alloc(&bounded_heap, 2 * KHEAP_PAGE_SIZE);
  assert(too_large == NULL && bounded_heap.mapped_pages == 0);
  assert(bounded_heap.last_error == KHEAP_OUT_OF_MEMORY);

  puts("heap native checks PASS");
  return 0;
}
