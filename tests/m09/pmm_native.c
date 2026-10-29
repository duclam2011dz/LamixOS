/* SPDX-License-Identifier: GPL-3.0-only */
#include "pmm.h"
#include <stdio.h>
#include <stdlib.h>

#define CHECK(condition)                                                       \
  do {                                                                         \
    if (!(condition)) {                                                        \
      fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition);           \
      return 1;                                                                \
    }                                                                          \
  } while (0)

int main(int argc, char **argv) {
  CHECK(argc == 2);
  FILE *file = fopen(argv[1], "rb");
  CHECK(file != NULL);
  uint8_t blob[4096];
  size_t size = fread(blob, 1, sizeof(blob), file);
  CHECK(!ferror(file) && !fclose(file) && size != sizeof(blob));

  lbp_view view = {0};
  CHECK(lbp_validate(blob, size, NULL, 0, &view) == LBP_OK);
  uint8_t storage[PMM_STATE_BYTES] = {0};
  pmm manager = {0};
  CHECK(pmm_init(&manager, &view, storage, sizeof(storage) - 1) ==
        PMM_STORAGE_TOO_SMALL);
  CHECK(pmm_init(&manager, &view, storage, sizeof(storage)) == PMM_OK);
  CHECK(manager.free_frames == 16 && manager.allocated_frames == 0);
  CHECK(pmm_free_frame(&manager, 0x130000) == PMM_NOT_ALLOCATED);
  CHECK(pmm_free_frame(&manager, PMM_PHYSICAL_LIMIT) == PMM_OUT_OF_RANGE);

  uint64_t allocated[16];
  for (unsigned i = 0; i < 16; ++i) {
    CHECK(pmm_alloc_frame(&manager, &allocated[i]) == PMM_OK);
    CHECK(allocated[i] == UINT64_C(0x200000) + i * PMM_PAGE_SIZE);
    CHECK(manager.free_frames == 15 - i && manager.allocated_frames == i + 1);
  }
  uint64_t untouched = UINT64_C(0xfeedface);
  CHECK(pmm_alloc_frame(&manager, &untouched) == PMM_OUT_OF_MEMORY);
  CHECK(untouched == UINT64_C(0xfeedface));
  for (unsigned i = 0; i < 16; ++i)
    CHECK(pmm_free_frame(&manager, allocated[i]) == PMM_OK);
  CHECK(manager.free_frames == 16 && manager.allocated_frames == 0);
  CHECK(pmm_free_frame(&manager, allocated[0]) == PMM_NOT_ALLOCATED);
  CHECK(pmm_free_frame(&manager, allocated[1] + 1) == PMM_INVALID_ARGUMENT);

  uint32_t random = UINT32_C(0x09a110c);
  uint8_t live[16] = {0};
  uint32_t live_count = 0;
  for (unsigned iteration = 0; iteration < 20000; ++iteration) {
    random = random * UINT32_C(1664525) + UINT32_C(1013904223);
    unsigned index = (random >> 16) & 15u;
    if (live[index]) {
      CHECK(pmm_free_frame(&manager, allocated[index]) == PMM_OK);
      live[index] = 0;
      --live_count;
    } else {
      uint64_t physical = 0;
      if (pmm_alloc_frame(&manager, &physical) == PMM_OK) {
        unsigned slot = 0;
        while (slot < 16 && live[slot])
          ++slot;
        CHECK(slot < 16);
        CHECK(physical >= UINT64_C(0x200000) &&
              physical < UINT64_C(0x210000));
        for (unsigned prior = 0; prior < 16; ++prior)
          CHECK(!live[prior] || allocated[prior] != physical);
        allocated[slot] = physical;
        live[slot] = 1;
        ++live_count;
      }
    }
    CHECK(manager.allocated_frames == live_count);
    CHECK(manager.free_frames + manager.allocated_frames == 16);
  }
  for (unsigned i = 0; i < 16; ++i)
    if (live[i])
      CHECK(pmm_free_frame(&manager, allocated[i]) == PMM_OK);
  CHECK(manager.free_frames == 16 && manager.allocated_frames == 0);
  puts("PASS: PMM bounds, reservations, OOM, ownership and 20000 stress steps");
  return 0;
}
