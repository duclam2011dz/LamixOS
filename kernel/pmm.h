/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef LAMIX_KERNEL_PMM_H
#define LAMIX_KERNEL_PMM_H

#include "lamix/boot/contract.h"

#define PMM_PAGE_SIZE UINT64_C(4096)
#define PMM_PHYSICAL_LIMIT UINT64_C(0x100000000)
#define PMM_FRAME_COUNT (PMM_PHYSICAL_LIMIT / PMM_PAGE_SIZE)
#define PMM_STATE_BYTES (PMM_FRAME_COUNT / 4)
#define PMM_BUDDY_MAX_ORDER 18u
#define PMM_BUDDY_NODE_COUNT (PMM_FRAME_COUNT * 2u - 1u)
#define PMM_BUDDY_STATE_BYTES ((PMM_BUDDY_NODE_COUNT + 3u) / 4u)
#define PMM_BUDDY_BOUNDARY_CAPACITY 16384u

typedef enum pmm_status {
  PMM_OK,
  PMM_INVALID_ARGUMENT,
  PMM_INVALID_MAP,
  PMM_STORAGE_TOO_SMALL,
  PMM_OUT_OF_MEMORY,
  PMM_OUT_OF_RANGE,
  PMM_NOT_ALLOCATED
} pmm_status;

typedef struct pmm {
  uint8_t *states;
  uint32_t free_frames;
  uint32_t allocated_frames;
  uint32_t next_hint;
  uint32_t initialized;
} pmm;

typedef struct pmm_buddy {
  uint8_t *nodes;
  uint32_t *boundaries;
  uint32_t boundary_count;
  uint32_t free_blocks[PMM_BUDDY_MAX_ORDER + 1u];
  uint32_t free_frames;
  uint32_t allocated_frames;
  uint32_t initialized;
} pmm_buddy;

typedef struct pmm_buddy_stats {
  uint32_t free_frames;
  uint32_t allocated_frames;
  uint64_t largest_free_block_bytes;
  uint32_t free_blocks[PMM_BUDDY_MAX_ORDER + 1u];
} pmm_buddy_stats;

pmm_status pmm_init(pmm *manager, const lbp_view *view, uint8_t *storage,
                    uint64_t storage_size);
pmm_status pmm_alloc_frame(pmm *manager, uint64_t *physical);
pmm_status pmm_free_frame(pmm *manager, uint64_t physical);

/* Experimental M12 backend. Order 0 is one 4 KiB frame; order 18 is 1 GiB. */
pmm_status pmm_buddy_init(pmm_buddy *manager, const lbp_view *view,
                          uint8_t *node_storage, uint64_t node_storage_size,
                          uint32_t *boundary_storage,
                          uint32_t boundary_capacity);
pmm_status pmm_buddy_alloc_order(pmm_buddy *manager, uint32_t order,
                                 uint64_t *physical);
pmm_status pmm_buddy_free_order(pmm_buddy *manager, uint64_t physical,
                                uint32_t order);
pmm_status pmm_buddy_alloc_frame(pmm_buddy *manager, uint64_t *physical);
pmm_status pmm_buddy_free_frame(pmm_buddy *manager, uint64_t physical);
pmm_status pmm_buddy_get_stats(const pmm_buddy *manager,
                               pmm_buddy_stats *stats);

#endif
