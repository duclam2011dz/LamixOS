/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef LAMIX_KERNEL_PMM_H
#define LAMIX_KERNEL_PMM_H

#include "lamix/boot/contract.h"

#define PMM_PAGE_SIZE UINT64_C(4096)
#define PMM_PHYSICAL_LIMIT UINT64_C(0x100000000)
#define PMM_FRAME_COUNT (PMM_PHYSICAL_LIMIT / PMM_PAGE_SIZE)
#define PMM_STATE_BYTES (PMM_FRAME_COUNT / 4)

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

pmm_status pmm_init(pmm *manager, const lbp_view *view, uint8_t *storage,
                    uint64_t storage_size);
pmm_status pmm_alloc_frame(pmm *manager, uint64_t *physical);
pmm_status pmm_free_frame(pmm *manager, uint64_t physical);

#endif
