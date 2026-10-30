/* SPDX-License-Identifier: GPL-3.0-only */
#include "heap.h"

#define KHEAP_MAGIC UINT32_C(0x4b484d31)
#define BLOCK_MAGIC UINT64_C(0x4b48454150424c4b)
#define BLOCK_FREE UINT64_C(1)
#define BLOCK_ALLOCATED UINT64_C(2)
#define NO_BLOCK UINT64_MAX
#define BLOCK_MIN_SIZE (sizeof(heap_block) + KHEAP_ALIGNMENT)

typedef struct heap_block {
  uint64_t magic;
  uint64_t cookie;
  uint64_t total_size;
  uint64_t previous_size;
  uint64_t free_previous;
  uint64_t free_next;
  uint64_t requested_size;
  uint64_t flags;
} heap_block;

_Static_assert(sizeof(heap_block) == 64, "heap block header size");
_Static_assert((sizeof(heap_block) & (KHEAP_ALIGNMENT - 1)) == 0,
               "heap block alignment");

static uint64_t mapped_bytes(const kheap *heap) {
  return heap->mapped_pages * KHEAP_PAGE_SIZE;
}

static uint64_t block_cookie(const kheap *heap, uint64_t offset,
                             const heap_block *block) {
  return heap->cookie_salt ^ BLOCK_MAGIC ^ offset ^ block->total_size ^
         block->previous_size ^ block->requested_size ^ block->flags;
}

static heap_block *block_at(const kheap *heap, uint64_t offset) {
  return (heap_block *)(heap->accessible_base + (uintptr_t)offset);
}

static void seal_block(const kheap *heap, uint64_t offset,
                       heap_block *block) {
  block->magic = BLOCK_MAGIC;
  block->cookie = block_cookie(heap, offset, block);
}

static void update_cookie(const kheap *heap, uint64_t offset,
                          heap_block *block) {
  block->cookie = block_cookie(heap, offset, block);
}

static int valid_block(const kheap *heap, uint64_t offset,
                       const heap_block *block) {
  uint64_t bytes = mapped_bytes(heap);
  if ((offset & (KHEAP_ALIGNMENT - 1)) || offset > bytes ||
      bytes - offset < sizeof(*block) || block->magic != BLOCK_MAGIC ||
      block->cookie != block_cookie(heap, offset, block) ||
      block->total_size < BLOCK_MIN_SIZE ||
      (block->total_size & (KHEAP_ALIGNMENT - 1)) ||
      block->total_size > bytes - offset ||
      (block->flags != BLOCK_FREE && block->flags != BLOCK_ALLOCATED))
    return 0;
  if (block->flags == BLOCK_FREE && block->requested_size != 0)
    return 0;
  if (block->flags == BLOCK_ALLOCATED &&
      (!block->requested_size ||
       block->requested_size > block->total_size - sizeof(*block)))
    return 0;
  return 1;
}

static int set_next_previous_size(kheap *heap, uint64_t offset,
                                  uint64_t size) {
  uint64_t end = offset + size;
  if (end == mapped_bytes(heap))
    return 1;
  if (end > mapped_bytes(heap) || mapped_bytes(heap) - end < sizeof(heap_block))
    return 0;
  heap_block *next = block_at(heap, end);
  if (!valid_block(heap, end, next))
    return 0;
  next->previous_size = size;
  update_cookie(heap, end, next);
  return 1;
}

static int unlink_free(kheap *heap, uint64_t offset) {
  heap_block *block = block_at(heap, offset);
  if (!valid_block(heap, offset, block) || block->flags != BLOCK_FREE)
    return 0;
  if (block->free_previous == NO_BLOCK) {
    if (heap->free_head != offset)
      return 0;
    heap->free_head = block->free_next;
  } else {
    if (block->free_previous >= offset)
      return 0;
    heap_block *previous = block_at(heap, block->free_previous);
    if (!valid_block(heap, block->free_previous, previous) ||
        previous->flags != BLOCK_FREE || previous->free_next != offset)
      return 0;
    previous->free_next = block->free_next;
  }
  if (block->free_next != NO_BLOCK) {
    if (block->free_next <= offset)
      return 0;
    heap_block *next = block_at(heap, block->free_next);
    if (!valid_block(heap, block->free_next, next) ||
        next->flags != BLOCK_FREE || next->free_previous != offset)
      return 0;
    next->free_previous = block->free_previous;
  }
  return 1;
}

static int insert_free(kheap *heap, uint64_t offset) {
  heap_block *block = block_at(heap, offset);
  if (!valid_block(heap, offset, block) || block->flags != BLOCK_FREE)
    return 0;
  uint64_t previous_offset = NO_BLOCK;
  uint64_t next_offset = heap->free_head;
  uint64_t traversed = 0;
  uint64_t limit = mapped_bytes(heap) / BLOCK_MIN_SIZE + 1;
  while (next_offset != NO_BLOCK && next_offset < offset) {
    if (++traversed > limit)
      return 0;
    heap_block *next = block_at(heap, next_offset);
    if (!valid_block(heap, next_offset, next) || next->flags != BLOCK_FREE)
      return 0;
    previous_offset = next_offset;
    next_offset = next->free_next;
  }
  if (next_offset == offset || previous_offset == offset)
    return 0;
  block->free_previous = previous_offset;
  block->free_next = next_offset;
  if (previous_offset == NO_BLOCK)
    heap->free_head = offset;
  else
    block_at(heap, previous_offset)->free_next = offset;
  if (next_offset != NO_BLOCK)
    block_at(heap, next_offset)->free_previous = offset;
  return 1;
}

static int layout_tail(kheap *heap, uint64_t *tail_offset,
                       heap_block **tail_block) {
  uint64_t bytes = mapped_bytes(heap);
  if (!bytes) {
    *tail_offset = 0;
    *tail_block = 0;
    return 1;
  }
  uint64_t offset = 0;
  uint64_t previous_size = 0;
  uint64_t iterations = 0;
  while (offset < bytes) {
    if (++iterations > bytes / BLOCK_MIN_SIZE + 1 ||
        bytes - offset < sizeof(heap_block))
      return 0;
    heap_block *block = block_at(heap, offset);
    if (!valid_block(heap, offset, block) ||
        block->previous_size != previous_size)
      return 0;
    previous_size = block->total_size;
    if (block->total_size > bytes - offset)
      return 0;
    *tail_offset = offset;
    *tail_block = block;
    offset += block->total_size;
  }
  return offset == bytes;
}

static uint64_t find_fit(kheap *heap, uint64_t required) {
  uint64_t offset = heap->free_head;
  uint64_t traversed = 0;
  uint64_t limit = mapped_bytes(heap) / BLOCK_MIN_SIZE + 1;
  while (offset != NO_BLOCK) {
    if (++traversed > limit)
      return NO_BLOCK;
    heap_block *block = block_at(heap, offset);
    if (!valid_block(heap, offset, block) || block->flags != BLOCK_FREE)
      return NO_BLOCK;
    if (block->total_size >= required)
      return offset;
    offset = block->free_next;
  }
  return NO_BLOCK;
}

static void poison(kheap *heap) {
  heap->poisoned = 1;
  heap->last_error = KHEAP_CORRUPT;
}

static int rollback_pages(kheap *heap, uint64_t first_page,
                          uint64_t end_page) {
  int okay = 1;
  while (end_page > first_page) {
    --end_page;
    uint64_t address = heap->virtual_base + end_page * KHEAP_PAGE_SIZE;
    if (!heap->operations.release_page(heap->operations.context, address))
      okay = 0;
  }
  if (!okay)
    poison(heap);
  return okay;
}

static int grow_heap(kheap *heap, uint64_t required) {
  uint64_t old_pages = heap->mapped_pages;
  uint64_t old_bytes = mapped_bytes(heap);
  uint64_t tail_offset = 0;
  heap_block *tail = 0;
  if (!layout_tail(heap, &tail_offset, &tail)) {
    poison(heap);
    return 0;
  }
  uint64_t tail_free = tail && tail->flags == BLOCK_FREE ? tail->total_size : 0;
  uint64_t missing = required > tail_free ? required - tail_free : 1;
  uint64_t pages = missing / KHEAP_PAGE_SIZE +
                   (missing % KHEAP_PAGE_SIZE != 0);
  if (!pages)
    pages = 1;
  uint64_t max_pages = heap->capacity_bytes / KHEAP_PAGE_SIZE;
  if (pages > max_pages - old_pages) {
    heap->last_error = KHEAP_OUT_OF_MEMORY;
    return 0;
  }

  uint64_t mapped_end = old_pages;
  for (; mapped_end < old_pages + pages; ++mapped_end) {
    uint64_t address = heap->virtual_base + mapped_end * KHEAP_PAGE_SIZE;
    void *accessible_page = 0;
    if (!heap->operations.map_page(heap->operations.context, address,
                                   &accessible_page) || !accessible_page ||
        ((uintptr_t)accessible_page & (KHEAP_PAGE_SIZE - 1))) {
      if (accessible_page || mapped_end > old_pages)
        (void)rollback_pages(heap, old_pages, mapped_end +
                              (accessible_page != 0));
      if (!heap->poisoned)
        heap->last_error = KHEAP_OUT_OF_MEMORY;
      return 0;
    }
    if (old_bytes == 0 && mapped_end == 0) {
      heap->accessible_base = (uintptr_t)accessible_page;
    } else if ((uintptr_t)accessible_page !=
               heap->accessible_base + mapped_end * KHEAP_PAGE_SIZE) {
      (void)rollback_pages(heap, old_pages, mapped_end + 1);
      poison(heap);
      return 0;
    }
  }

  heap->mapped_pages = old_pages + pages;
  uint64_t added_bytes = pages * KHEAP_PAGE_SIZE;
  if (tail && tail->flags == BLOCK_FREE) {
    tail->total_size += added_bytes;
    update_cookie(heap, tail_offset, tail);
  } else {
    heap_block *block = block_at(heap, old_bytes);
    *block = (heap_block){.total_size = added_bytes,
                          .previous_size = tail ? tail->total_size : 0,
                          .free_previous = NO_BLOCK,
                          .free_next = NO_BLOCK,
                          .requested_size = 0,
                          .flags = BLOCK_FREE};
    seal_block(heap, old_bytes, block);
    if (!insert_free(heap, old_bytes)) {
      poison(heap);
      return 0;
    }
  }
  heap->free_block_bytes += added_bytes;
  if (!set_next_previous_size(heap, tail && tail->flags == BLOCK_FREE
                                        ? tail_offset
                                        : old_bytes,
                              tail && tail->flags == BLOCK_FREE
                                  ? tail->total_size
                                  : added_bytes)) {
    poison(heap);
    return 0;
  }
  return 1;
}

kheap_status kheap_init(kheap *heap, const kheap_operations *operations,
                        uint64_t virtual_base, uint64_t capacity_bytes) {
  if (!heap || !operations || !operations->map_page ||
      !operations->release_page || (virtual_base & (KHEAP_PAGE_SIZE - 1)) ||
      !capacity_bytes || (capacity_bytes & (KHEAP_PAGE_SIZE - 1)) ||
      virtual_base > UINT64_MAX - capacity_bytes)
    return KHEAP_INVALID_ARGUMENT;
  *heap = (kheap){.operations = *operations,
                  .virtual_base = (uintptr_t)virtual_base,
                  .capacity_bytes = capacity_bytes,
                  .free_head = NO_BLOCK,
                  .cookie_salt = UINT64_C(0x4d31314845415031) ^ virtual_base ^
                                 capacity_bytes,
                  .last_error = KHEAP_OK,
                  .initialized = KHEAP_MAGIC};
  return KHEAP_OK;
}

void *kheap_alloc(kheap *heap, size_t size) {
  if (!heap || heap->initialized != KHEAP_MAGIC) {
    if (heap)
      heap->last_error = KHEAP_NOT_INITIALIZED;
    return 0;
  }
  if (heap->poisoned) {
    heap->last_error = KHEAP_CORRUPT;
    return 0;
  }
  if (!size) {
    heap->last_error = KHEAP_INVALID_ARGUMENT;
    return 0;
  }
  if ((uint64_t)size > UINT64_MAX - sizeof(heap_block) -
                            (KHEAP_ALIGNMENT - 1)) {
    heap->last_error = KHEAP_OUT_OF_MEMORY;
    return 0;
  }
  uint64_t required = ((uint64_t)size + sizeof(heap_block) +
                       (KHEAP_ALIGNMENT - 1)) &
                      ~(KHEAP_ALIGNMENT - 1);
  if (required < BLOCK_MIN_SIZE)
    required = BLOCK_MIN_SIZE;

  uint64_t offset = find_fit(heap, required);
  if (offset == NO_BLOCK) {
    if (heap->poisoned || !grow_heap(heap, required))
      return 0;
    offset = find_fit(heap, required);
    if (offset == NO_BLOCK) {
      poison(heap);
      return 0;
    }
  }
  heap_block *block = block_at(heap, offset);
  uint64_t original_size = block->total_size;
  if (!unlink_free(heap, offset)) {
    poison(heap);
    return 0;
  }
  uint64_t allocated_size = required;
  uint64_t remainder_size = original_size - required;
  if (remainder_size < BLOCK_MIN_SIZE) {
    allocated_size = original_size;
    remainder_size = 0;
  }
  block->total_size = allocated_size;
  block->requested_size = size;
  block->flags = BLOCK_ALLOCATED;
  block->free_previous = NO_BLOCK;
  block->free_next = NO_BLOCK;
  seal_block(heap, offset, block);
  if (remainder_size) {
    uint64_t remainder_offset = offset + allocated_size;
    heap_block *remainder = block_at(heap, remainder_offset);
    *remainder = (heap_block){.total_size = remainder_size,
                              .previous_size = allocated_size,
                              .free_previous = NO_BLOCK,
                              .free_next = NO_BLOCK,
                              .requested_size = 0,
                              .flags = BLOCK_FREE};
    seal_block(heap, remainder_offset, remainder);
    if (!insert_free(heap, remainder_offset) ||
        !set_next_previous_size(heap, remainder_offset, remainder_size)) {
      poison(heap);
      return 0;
    }
  } else if (!set_next_previous_size(heap, offset, allocated_size)) {
    poison(heap);
    return 0;
  }
  heap->free_block_bytes -= allocated_size;
  heap->live_allocations++;
  heap->live_bytes += size;
  if (heap->live_bytes > heap->peak_live_bytes)
    heap->peak_live_bytes = heap->live_bytes;
  heap->allocation_count++;
  heap->last_error = KHEAP_OK;
  return (void *)(heap->accessible_base + (uintptr_t)offset + sizeof(*block));
}

void *kheap_calloc(kheap *heap, size_t count, size_t size) {
  if (!count || !size) {
    if (heap && heap->initialized == KHEAP_MAGIC)
      heap->last_error = KHEAP_INVALID_ARGUMENT;
    return 0;
  }
  if (size > SIZE_MAX / count) {
    if (heap && heap->initialized == KHEAP_MAGIC)
      heap->last_error = KHEAP_OUT_OF_MEMORY;
    return 0;
  }
  size_t bytes = count * size;
  void *memory = kheap_alloc(heap, bytes);
  if (!memory)
    return 0;
  uint8_t *cursor = (uint8_t *)memory;
  for (size_t i = 0; i < bytes; ++i)
    cursor[i] = 0;
  return memory;
}

kheap_status kheap_free(kheap *heap, void *pointer) {
  if (!heap || heap->initialized != KHEAP_MAGIC)
    return KHEAP_NOT_INITIALIZED;
  if (heap->poisoned)
    return KHEAP_CORRUPT;
  if (!pointer) {
    heap->last_error = KHEAP_OK;
    return KHEAP_OK;
  }
  if (!heap->accessible_base ||
      (uintptr_t)pointer < heap->accessible_base + sizeof(heap_block) ||
      (uintptr_t)pointer >= heap->accessible_base + mapped_bytes(heap) ||
      ((uintptr_t)pointer & (KHEAP_ALIGNMENT - 1))) {
    heap->invalid_free_count++;
    heap->last_error = KHEAP_INVALID_POINTER;
    return KHEAP_INVALID_POINTER;
  }
  uint64_t offset = (uintptr_t)pointer - heap->accessible_base -
                    sizeof(heap_block);
  if (offset & (KHEAP_ALIGNMENT - 1)) {
    heap->invalid_free_count++;
    heap->last_error = KHEAP_INVALID_POINTER;
    return KHEAP_INVALID_POINTER;
  }
  heap_block *block = block_at(heap, offset);
  if (block->magic == BLOCK_MAGIC && block->flags == BLOCK_FREE &&
      block->cookie == block_cookie(heap, offset, block)) {
    heap->double_free_count++;
    heap->last_error = KHEAP_DOUBLE_FREE;
    return KHEAP_DOUBLE_FREE;
  }
  if (!valid_block(heap, offset, block) || block->flags != BLOCK_ALLOCATED) {
    heap->invalid_free_count++;
    heap->last_error = KHEAP_INVALID_POINTER;
    return KHEAP_INVALID_POINTER;
  }

  uint64_t original_size = block->total_size;
  uint64_t requested_size = block->requested_size;
  block->flags = BLOCK_FREE;
  block->requested_size = 0;
  block->free_previous = NO_BLOCK;
  block->free_next = NO_BLOCK;
  update_cookie(heap, offset, block);
  heap->free_block_bytes += original_size;
  heap->live_allocations--;
  heap->live_bytes -= requested_size;
  heap->free_count++;

  if (block->previous_size) {
    uint64_t previous_offset = offset - block->previous_size;
    heap_block *previous = block_at(heap, previous_offset);
    if (!valid_block(heap, previous_offset, previous) ||
        previous->total_size != block->previous_size) {
      poison(heap);
      return KHEAP_CORRUPT;
    }
    if (previous->flags == BLOCK_FREE) {
      if (!unlink_free(heap, previous_offset)) {
        poison(heap);
        return KHEAP_CORRUPT;
      }
      previous->total_size += block->total_size;
      update_cookie(heap, previous_offset, previous);
      offset = previous_offset;
      block = previous;
    }
  }
  if (!set_next_previous_size(heap, offset, block->total_size)) {
    poison(heap);
    return KHEAP_CORRUPT;
  }
  uint64_t next_offset = offset + block->total_size;
  if (next_offset < mapped_bytes(heap)) {
    heap_block *next = block_at(heap, next_offset);
    if (!valid_block(heap, next_offset, next) ||
        next->previous_size != block->total_size) {
      poison(heap);
      return KHEAP_CORRUPT;
    }
    if (next->flags == BLOCK_FREE) {
      if (!unlink_free(heap, next_offset)) {
        poison(heap);
        return KHEAP_CORRUPT;
      }
      block->total_size += next->total_size;
      update_cookie(heap, offset, block);
    }
  }
  if (!set_next_previous_size(heap, offset, block->total_size) ||
      !insert_free(heap, offset)) {
    poison(heap);
    return KHEAP_CORRUPT;
  }
  heap->last_error = KHEAP_OK;
  return KHEAP_OK;
}

kheap_status kheap_get_stats(const kheap *heap, kheap_stats *stats) {
  if (!heap || !stats || heap->initialized != KHEAP_MAGIC)
    return KHEAP_INVALID_ARGUMENT;
  *stats = (kheap_stats){.mapped_pages = heap->mapped_pages,
                         .live_allocations = heap->live_allocations,
                         .live_bytes = heap->live_bytes,
                         .peak_live_bytes = heap->peak_live_bytes,
                         .free_block_bytes = heap->free_block_bytes,
                         .allocation_count = heap->allocation_count,
                         .free_count = heap->free_count,
                         .invalid_free_count = heap->invalid_free_count,
                         .double_free_count = heap->double_free_count,
                         .last_error = heap->last_error};
  return KHEAP_OK;
}
