/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef LAMIX_KERNEL_HEAP_H
#define LAMIX_KERNEL_HEAP_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KHEAP_PAGE_SIZE UINT64_C(4096)
#define KHEAP_ALIGNMENT UINT64_C(16)
#define KHEAP_VIRTUAL_BASE UINT64_C(0xffffe00000000000)
#define KHEAP_VIRTUAL_SIZE UINT64_C(0x40000000)

typedef enum kheap_status {
  KHEAP_OK,
  KHEAP_INVALID_ARGUMENT,
  KHEAP_INVALID_POINTER,
  KHEAP_DOUBLE_FREE,
  KHEAP_OUT_OF_MEMORY,
  KHEAP_BACKEND_FAILURE,
  KHEAP_CORRUPT,
  KHEAP_NOT_INITIALIZED
} kheap_status;

typedef struct kheap_operations {
  void *context;
  int (*map_page)(void *context, uint64_t virtual_address,
                  void **accessible_page);
  int (*release_page)(void *context, uint64_t virtual_address);
} kheap_operations;

typedef struct kheap_stats {
  uint64_t mapped_pages;
  uint64_t live_allocations;
  uint64_t live_bytes;
  uint64_t peak_live_bytes;
  uint64_t free_block_bytes;
  uint64_t allocation_count;
  uint64_t free_count;
  uint64_t invalid_free_count;
  uint64_t double_free_count;
  kheap_status last_error;
} kheap_stats;

typedef struct kheap {
  kheap_operations operations;
  uintptr_t virtual_base;
  uintptr_t accessible_base;
  uint64_t capacity_bytes;
  uint64_t mapped_pages;
  uint64_t free_head;
  uint64_t live_allocations;
  uint64_t live_bytes;
  uint64_t peak_live_bytes;
  uint64_t free_block_bytes;
  uint64_t allocation_count;
  uint64_t free_count;
  uint64_t invalid_free_count;
  uint64_t double_free_count;
  uint64_t cookie_salt;
  kheap_status last_error;
  uint32_t initialized;
  uint32_t poisoned;
} kheap;

kheap_status kheap_init(kheap *heap, const kheap_operations *operations,
                        uint64_t virtual_base, uint64_t capacity_bytes);
void *kheap_alloc(kheap *heap, size_t size);
void *kheap_calloc(kheap *heap, size_t count, size_t size);
kheap_status kheap_free(kheap *heap, void *pointer);
kheap_status kheap_get_stats(const kheap *heap, kheap_stats *stats);

#ifdef __cplusplus
}
#endif

#endif
