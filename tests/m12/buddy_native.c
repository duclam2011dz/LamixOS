/* SPDX-License-Identifier: GPL-3.0-only */
#include "pmm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition)                                                       \
  do {                                                                         \
    if (!(condition)) {                                                        \
      fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition);           \
      return 1;                                                                \
    }                                                                          \
  } while (0)

static uint8_t node_storage[PMM_BUDDY_STATE_BYTES];
static uint32_t boundary_storage[PMM_BUDDY_BOUNDARY_CAPACITY];

static int read_view(const char *path, uint8_t **blob, size_t *size,
                     lbp_view *view) {
  FILE *file = fopen(path, "rb");
  if (!file)
    return 0;
  if (fseek(file, 0, SEEK_END) != 0)
    return 0;
  long length = ftell(file);
  if (length <= 0 || fseek(file, 0, SEEK_SET) != 0)
    return 0;
  *blob = malloc((size_t)length);
  if (!*blob)
    return 0;
  *size = fread(*blob, 1, (size_t)length, file);
  int okay = *size == (size_t)length && !ferror(file) && fclose(file) == 0 &&
             lbp_validate(*blob, *size, NULL, 0, view) == LBP_OK;
  return okay;
}

static int init_buddy(const char *path, pmm_buddy *buddy, uint8_t **blob,
                      size_t *size, lbp_view *view) {
  if (!read_view(path, blob, size, view))
    return 0;
  return pmm_buddy_init(buddy, view, node_storage, sizeof(node_storage),
                        boundary_storage, PMM_BUDDY_BOUNDARY_CAPACITY) ==
         PMM_OK;
}

int main(int argc, char **argv) {
  CHECK(argc == 2);
  pmm_buddy manager = {0};
  uint8_t *blob = NULL;
  size_t blob_size = 0;
  lbp_view view = {0};
  if (argc == 2 && strcmp(argv[1], "large.bin") == 0) {
    CHECK(init_buddy(argv[1], &manager, &blob, &blob_size, &view));
    CHECK(manager.free_frames == (UINT32_C(1) << 18) + 16u);
    uint64_t large = 0;
    CHECK(pmm_buddy_alloc_order(&manager, 18, &large) == PMM_OK);
    CHECK(large == UINT64_C(0x40000000));
    CHECK(pmm_buddy_free_order(&manager, large, 18) == PMM_OK);
    CHECK(manager.allocated_frames == 0);
    free(blob);
    puts("PASS: 1 GiB order18 under ASan/UBSan");
    return 0;
  }
  CHECK(read_view(argv[1], &blob, &blob_size, &view));
  node_storage[0] = 0xa5;
  CHECK(pmm_buddy_init(&manager, &view, node_storage,
                       sizeof(node_storage) - 1u, boundary_storage,
                       PMM_BUDDY_BOUNDARY_CAPACITY) == PMM_STORAGE_TOO_SMALL);
  CHECK(node_storage[0] == 0xa5);
  CHECK(pmm_buddy_init(&manager, &view, node_storage, sizeof(node_storage),
                       boundary_storage, 1) == PMM_STORAGE_TOO_SMALL);
  CHECK(node_storage[0] == 0xa5);
  free(blob);
  blob = NULL;
  CHECK(init_buddy(argv[1], &manager, &blob, &blob_size, &view));
  pmm_buddy_stats initial;
  CHECK(pmm_buddy_get_stats(&manager, &initial) == PMM_OK);
  CHECK(initial.free_frames == 16 && initial.allocated_frames == 0);
  CHECK(initial.free_blocks[4] == 1);

  uint64_t block = UINT64_MAX;
  CHECK(pmm_buddy_alloc_order(&manager, 2, &block) == PMM_OK);
  CHECK(block == UINT64_C(0x200000));
  CHECK(manager.free_frames == 12 && manager.allocated_frames == 4);
  CHECK(pmm_buddy_free_order(&manager, block, 1) == PMM_NOT_ALLOCATED);
  CHECK(pmm_buddy_free_order(&manager, block + PMM_PAGE_SIZE, 2) ==
        PMM_INVALID_ARGUMENT);
  CHECK(pmm_buddy_free_order(&manager, block, 2) == PMM_OK);
  CHECK(pmm_buddy_free_order(&manager, block, 2) == PMM_NOT_ALLOCATED);
  pmm_buddy_stats restored;
  CHECK(pmm_buddy_get_stats(&manager, &restored) == PMM_OK);
  CHECK(restored.free_frames == initial.free_frames &&
        restored.allocated_frames == 0 &&
        restored.largest_free_block_bytes == initial.largest_free_block_bytes);

  uint64_t untouched = UINT64_C(0xfeedface);
  CHECK(pmm_buddy_alloc_order(&manager, 4, &block) == PMM_OK);
  CHECK((block & ((PMM_PAGE_SIZE << 4) - 1u)) == 0);
  CHECK(manager.allocated_frames == 16 && manager.free_frames == 0);
  CHECK(pmm_buddy_alloc_frame(&manager, &untouched) == PMM_OUT_OF_MEMORY);
  CHECK(pmm_buddy_free_order(&manager, block, 4) == PMM_OK);
  CHECK(manager.free_frames == 16 && manager.allocated_frames == 0);

  uint64_t frames[16];
  for (unsigned i = 0; i < 16; ++i)
    CHECK(pmm_buddy_alloc_frame(&manager, &frames[i]) == PMM_OK);
  untouched = UINT64_C(0xfeedface);
  CHECK(pmm_buddy_alloc_frame(&manager, &untouched) == PMM_OUT_OF_MEMORY);
  CHECK(untouched == UINT64_C(0xfeedface));
  for (unsigned i = 0; i < 16; ++i)
    CHECK(pmm_buddy_free_frame(&manager, frames[i]) == PMM_OK);
  CHECK(pmm_buddy_free_frame(&manager, frames[0]) == PMM_NOT_ALLOCATED);
  CHECK(pmm_buddy_alloc_order(&manager, PMM_BUDDY_MAX_ORDER + 1u, &block) ==
        PMM_INVALID_ARGUMENT);
  CHECK(pmm_buddy_free_order(&manager, PMM_PHYSICAL_LIMIT, 0) ==
        PMM_OUT_OF_RANGE);

  uint32_t random = UINT32_C(0x12badd1e);
  uint64_t live_frames[16] = {0};
  uint8_t live[16] = {0};
  for (unsigned iteration = 0; iteration < 20000; ++iteration) {
    random = random * UINT32_C(1664525) + UINT32_C(1013904223);
    unsigned slot = (random >> 16) & 15u;
    if (live[slot]) {
      CHECK(pmm_buddy_free_frame(&manager, live_frames[slot]) == PMM_OK);
      live[slot] = 0;
    } else {
      uint64_t physical = 0;
      if (pmm_buddy_alloc_frame(&manager, &physical) == PMM_OK) {
        CHECK(physical >= UINT64_C(0x200000) &&
              physical < UINT64_C(0x210000));
        for (unsigned prior = 0; prior < 16; ++prior)
          CHECK(!live[prior] || live_frames[prior] != physical);
        live_frames[slot] = physical;
        live[slot] = 1;
      }
    }
    pmm_buddy_stats current;
    CHECK(pmm_buddy_get_stats(&manager, &current) == PMM_OK);
    uint32_t live_count = 0;
    for (unsigned i = 0; i < 16; ++i)
      live_count += live[i] != 0;
    CHECK(current.allocated_frames == live_count &&
          current.free_frames + current.allocated_frames == 16);
  }
  for (unsigned i = 0; i < 16; ++i)
    if (live[i])
      CHECK(pmm_buddy_free_frame(&manager, live_frames[i]) == PMM_OK);
  CHECK(manager.free_frames == 16 && manager.allocated_frames == 0);
  free(blob);

  CHECK(init_buddy("adjacent.bin", &manager, &blob, &blob_size, &view));
  CHECK(manager.free_frames == 4);
  CHECK(pmm_buddy_alloc_order(&manager, 2, &block) == PMM_OUT_OF_MEMORY);
  CHECK(pmm_buddy_alloc_order(&manager, 1, &block) == PMM_OK);
  CHECK(block == UINT64_C(0x200000));
  uint64_t second = 0;
  CHECK(pmm_buddy_alloc_order(&manager, 1, &second) == PMM_OK);
  CHECK(second == UINT64_C(0x202000));
  free(blob);

  CHECK(init_buddy("holes.bin", &manager, &blob, &blob_size, &view));
  CHECK(manager.free_frames == 4);
  CHECK(pmm_buddy_alloc_order(&manager, 2, &block) == PMM_OUT_OF_MEMORY);
  free(blob);

  CHECK(init_buddy("large.bin", &manager, &blob, &blob_size, &view));
  CHECK(manager.free_frames == (UINT32_C(1) << 18) + 16u);
  CHECK(pmm_buddy_alloc_order(&manager, 18, &block) == PMM_OK);
  CHECK(block == UINT64_C(0x40000000));
  CHECK(manager.allocated_frames == (UINT32_C(1) << 18));
  CHECK(pmm_buddy_free_order(&manager, block, 18) == PMM_OK);
  CHECK(manager.allocated_frames == 0);
  free(blob);

  puts("PASS: buddy extents, reservations, order bounds, OOM, coalescing, 1 GiB and 20000 stress steps");
  return 0;
}
