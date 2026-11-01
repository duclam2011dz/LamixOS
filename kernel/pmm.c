/* SPDX-License-Identifier: GPL-3.0-only */
#include "pmm.h"

#define STATE_UNAVAILABLE 0u
#define STATE_FREE 1u
#define STATE_ALLOCATED 2u
#define TABLE_PREFIX 32u
#define TABLE_LIMIT UINT64_C(32768)
#define PMM_MAGIC UINT32_C(0x504d4d39)
#define PMM_BUDDY_MAGIC UINT32_C(0x504d4231)

enum {
  BUDDY_NODE_MIXED = 0,
  BUDDY_NODE_FREE = 1,
  BUDDY_NODE_ALLOCATED = 2,
  BUDDY_NODE_BLOCKED = 3
};

typedef struct pmm_table {
  const uint8_t *entries;
  uint64_t count;
  uint32_t stride;
} pmm_table;

static uint8_t state_get(const uint8_t *states, uint32_t frame) {
  uint32_t shift = (frame & 3u) * 2u;
  return (uint8_t)((states[frame >> 2] >> shift) & 3u);
}

static void state_set(uint8_t *states, uint32_t frame, uint8_t state) {
  uint32_t shift = (frame & 3u) * 2u;
  uint8_t mask = (uint8_t)(3u << shift);
  states[frame >> 2] = (uint8_t)((states[frame >> 2] & (uint8_t)~mask) |
                                  (uint8_t)(state << shift));
}

static int range_end(uint64_t base, uint64_t length, uint64_t *end) {
  if (!length || base > UINT64_MAX - length)
    return 0;
  *end = base + length;
  return 1;
}

static int table_shape(const lbp_view *view, const lbp_record *record,
                       uint32_t wanted, pmm_table *table) {
  const uint8_t *bytes = (const uint8_t *)record;
  uint64_t size = record->size;
  if (record->type != wanted || size < TABLE_PREFIX ||
      *(const uint32_t *)(bytes + 16) != 1)
    return 0;
  uint32_t stride = *(const uint32_t *)(bytes + 20);
  uint64_t count = *(const uint64_t *)(bytes + 24);
  if (stride < sizeof(lbp_memory) || (stride & 7u) || count > TABLE_LIMIT ||
      count > (size - TABLE_PREFIX) / stride ||
      TABLE_PREFIX + count * stride > size ||
      (uint64_t)(bytes - view->bytes) > view->size ||
      size > view->size - (uint64_t)(bytes - view->bytes))
    return 0;
  table->entries = bytes + TABLE_PREFIX;
  table->count = count;
  table->stride = stride;
  return 1;
}

static int find_table(const lbp_view *view, uint32_t wanted,
                      pmm_table *table) {
  if (!view || !view->bytes || view->size < sizeof(lbp_header) ||
      view->count > LBP_MAX_RECORDS)
    return 0;
  for (uint32_t i = 0; i < view->count; ++i) {
    uint64_t offset = 0;
    if (lbp_record_at(view, i, &offset) != LBP_OK ||
        offset > view->size - sizeof(lbp_record))
      return 0;
    const lbp_record *record = (const lbp_record *)(view->bytes + offset);
    if (record->type == wanted)
      return table_shape(view, record, wanted, table);
  }
  return 0;
}

static int validate_memory_table(const pmm_table *table) {
  uint64_t previous_end = 0;
  for (uint64_t i = 0; i < table->count; ++i) {
    const lbp_memory *memory =
        (const lbp_memory *)(table->entries + i * table->stride);
    uint64_t end = 0;
    if (!range_end(memory->base, memory->length, &end) ||
        (memory->base & (PMM_PAGE_SIZE - 1)) ||
        (memory->length & (PMM_PAGE_SIZE - 1)) ||
        memory->base < previous_end || memory->type < LBP_USABLE ||
        memory->type > LBP_RUNTIME || (memory->flags & ~3u))
      return 0;
    if ((memory->flags & LBP_MEMORY_NONVOLATILE) &&
        (memory->type == LBP_USABLE || memory->type == LBP_LOADER_RECLAIM ||
         memory->type == LBP_ACPI_RECLAIM))
      return 0;
    previous_end = end;
  }
  return 1;
}

static int validate_reservations(const pmm_table *table) {
  for (uint64_t i = 0; i < table->count; ++i) {
    const lbp_reservation *reservation =
        (const lbp_reservation *)(table->entries + i * table->stride);
    uint64_t end = 0;
    if (!range_end(reservation->base, reservation->length, &end) ||
        (reservation->base & (PMM_PAGE_SIZE - 1)) ||
        (reservation->length & (PMM_PAGE_SIZE - 1)) ||
        reservation->reserved || reservation->purpose < LBP_PURPOSE_KERNEL ||
        reservation->purpose > LBP_PURPOSE_LOADER ||
        reservation->release != (reservation->purpose <= LBP_PURPOSE_MODULE
                                      ? LBP_RELEASE_NEVER
                                  : reservation->purpose == LBP_PURPOSE_BOOTINFO
                                      ? LBP_RELEASE_COPIED
                                      : LBP_RELEASE_REPLACED))
      return 0;
    for (uint64_t j = 0; j < i; ++j) {
      const lbp_reservation *prior =
          (const lbp_reservation *)(table->entries + j * table->stride);
      if (reservation->base < prior->base + prior->length &&
          prior->base < end)
        return 0;
    }
  }
  return 1;
}

static void mark_range(uint8_t *states, uint64_t base, uint64_t length,
                       uint8_t state) {
  uint64_t end = base + length;
  if (base >= PMM_PHYSICAL_LIMIT)
    return;
  if (end > PMM_PHYSICAL_LIMIT)
    end = PMM_PHYSICAL_LIMIT;
  uint32_t first = (uint32_t)(base / PMM_PAGE_SIZE);
  uint32_t last = (uint32_t)(end / PMM_PAGE_SIZE);
  for (uint32_t frame = first; frame < last; ++frame)
    state_set(states, frame, state);
}

pmm_status pmm_init(pmm *manager, const lbp_view *view, uint8_t *storage,
                    uint64_t storage_size) {
  if (!manager || !view || !storage)
    return PMM_INVALID_ARGUMENT;
  if (storage_size < PMM_STATE_BYTES)
    return PMM_STORAGE_TOO_SMALL;
  pmm_table memory = {0}, reservations = {0};
  if (!find_table(view, LBP_MEMORY_MAP, &memory) ||
      !find_table(view, LBP_RESERVATIONS, &reservations) ||
      !validate_memory_table(&memory) || !validate_reservations(&reservations))
    return PMM_INVALID_MAP;

  for (uint64_t i = 0; i < PMM_STATE_BYTES; ++i)
    storage[i] = 0;
  for (uint64_t i = 0; i < memory.count; ++i) {
    const lbp_memory *entry =
        (const lbp_memory *)(memory.entries + i * memory.stride);
    if (entry->type == LBP_USABLE)
      mark_range(storage, entry->base, entry->length, STATE_FREE);
  }
  for (uint64_t i = 0; i < reservations.count; ++i) {
    const lbp_reservation *entry =
        (const lbp_reservation *)(reservations.entries + i * reservations.stride);
    mark_range(storage, entry->base, entry->length, STATE_UNAVAILABLE);
  }
  pmm initialized = {.states = storage,
                     .free_frames = 0,
                     .allocated_frames = 0,
                     .next_hint = 0,
                     .initialized = PMM_MAGIC};
  for (uint32_t frame = 0; frame < PMM_FRAME_COUNT; ++frame)
    initialized.free_frames += state_get(storage, frame) == STATE_FREE;
  *manager = initialized;
  return PMM_OK;
}

pmm_status pmm_alloc_frame(pmm *manager, uint64_t *physical) {
  if (!manager || !physical || manager->initialized != PMM_MAGIC ||
      !manager->states)
    return PMM_INVALID_ARGUMENT;
  for (uint32_t scanned = 0; scanned < PMM_FRAME_COUNT; ++scanned) {
    uint32_t frame = manager->next_hint + scanned;
    if (frame >= PMM_FRAME_COUNT)
      frame -= PMM_FRAME_COUNT;
    if (state_get(manager->states, frame) == STATE_FREE) {
      state_set(manager->states, frame, STATE_ALLOCATED);
      manager->free_frames--;
      manager->allocated_frames++;
      manager->next_hint = frame + 1 == PMM_FRAME_COUNT ? 0 : frame + 1;
      *physical = (uint64_t)frame * PMM_PAGE_SIZE;
      return PMM_OK;
    }
  }
  return PMM_OUT_OF_MEMORY;
}

pmm_status pmm_alloc_frame_below(pmm *manager, uint64_t exclusive_limit,
                                 uint64_t *physical) {
  if (!manager || !physical || manager->initialized != PMM_MAGIC ||
      !manager->states || !exclusive_limit)
    return PMM_INVALID_ARGUMENT;
  uint64_t limit = exclusive_limit < PMM_PHYSICAL_LIMIT
                       ? exclusive_limit : PMM_PHYSICAL_LIMIT;
  uint32_t frames = (uint32_t)(limit / PMM_PAGE_SIZE);
  for (uint32_t frame = 1; frame < frames; ++frame) {
    if (state_get(manager->states, frame) != STATE_FREE) continue;
    state_set(manager->states, frame, STATE_ALLOCATED);
    --manager->free_frames;
    ++manager->allocated_frames;
    *physical = (uint64_t)frame * PMM_PAGE_SIZE;
    return PMM_OK;
  }
  return PMM_OUT_OF_MEMORY;
}

pmm_status pmm_free_frame(pmm *manager, uint64_t physical) {
  if (!manager || manager->initialized != PMM_MAGIC || !manager->states)
    return PMM_INVALID_ARGUMENT;
  if (physical & (PMM_PAGE_SIZE - 1))
    return PMM_INVALID_ARGUMENT;
  if (physical >= PMM_PHYSICAL_LIMIT)
    return PMM_OUT_OF_RANGE;
  uint32_t frame = (uint32_t)(physical / PMM_PAGE_SIZE);
  if (state_get(manager->states, frame) != STATE_ALLOCATED)
    return PMM_NOT_ALLOCATED;
  state_set(manager->states, frame, STATE_FREE);
  manager->free_frames++;
  manager->allocated_frames--;
  if (frame < manager->next_hint)
    manager->next_hint = frame;
  return PMM_OK;
}

static uint8_t buddy_node_get(const uint8_t *nodes, uint32_t index) {
  uint32_t shift = (index & 3u) * 2u;
  return (uint8_t)((nodes[index >> 2] >> shift) & 3u);
}

static void buddy_node_set(uint8_t *nodes, uint32_t index, uint8_t state) {
  uint32_t shift = (index & 3u) * 2u;
  uint8_t mask = (uint8_t)(3u << shift);
  nodes[index >> 2] = (uint8_t)((nodes[index >> 2] & (uint8_t)~mask) |
                                (uint8_t)(state << shift));
}

static void buddy_node_info(uint32_t index, uint32_t *start,
                            uint32_t *order) {
  uint32_t level = 0;
  uint32_t cursor = index + 1u;
  while (cursor >>= 1u)
    ++level;
  uint32_t first = (UINT32_C(1) << level) - 1u;
  *order = 20u - level;
  *start = (index - first) << *order;
}

static int buddy_boundary_inside(const pmm_buddy *manager, uint32_t start,
                                 uint32_t end) {
  uint32_t low = 0, high = manager->boundary_count;
  while (low < high) {
    uint32_t middle = low + (high - low) / 2u;
    if (manager->boundaries[middle] <= start)
      low = middle + 1u;
    else
      high = middle;
  }
  return low < manager->boundary_count && manager->boundaries[low] < end;
}

static void buddy_mark_range(uint8_t *nodes, uint64_t base, uint64_t length,
                             uint8_t state) {
  uint64_t end = base + length;
  if (base >= PMM_PHYSICAL_LIMIT)
    return;
  if (end > PMM_PHYSICAL_LIMIT)
    end = PMM_PHYSICAL_LIMIT;
  uint32_t first = (uint32_t)(base / PMM_PAGE_SIZE);
  uint32_t last = (uint32_t)(end / PMM_PAGE_SIZE);
  for (uint32_t frame = first; frame < last; ++frame)
    buddy_node_set(nodes, (uint32_t)(PMM_FRAME_COUNT - 1u + frame), state);
}

static int buddy_add_boundary(uint32_t *storage, uint32_t capacity,
                              uint32_t *count, uint64_t address) {
  if (address > PMM_PHYSICAL_LIMIT || (address & (PMM_PAGE_SIZE - 1u)) ||
      *count >= capacity)
    return 0;
  storage[(*count)++] = (uint32_t)(address / PMM_PAGE_SIZE);
  return 1;
}

static void buddy_sort_boundaries(uint32_t *values, uint32_t count) {
  /* Shell sort keeps initialization self-contained and has bounded storage. */
  for (uint32_t gap = count / 2u; gap; gap /= 2u) {
    for (uint32_t i = gap; i < count; ++i) {
      uint32_t value = values[i], j = i;
      while (j >= gap && values[j - gap] > value) {
        values[j] = values[j - gap];
        j -= gap;
      }
      values[j] = value;
    }
  }
}

static void buddy_build_tree(pmm_buddy *manager) {
  const uint32_t first_leaf = (uint32_t)(PMM_FRAME_COUNT - 1u);
  for (uint32_t index = first_leaf; index-- > 0;) {
    uint32_t start = 0, order = 0;
    buddy_node_info(index, &start, &order);
    uint32_t left = index * 2u + 1u;
    uint32_t right = left + 1u;
    uint8_t left_state = buddy_node_get(manager->nodes, left);
    uint8_t right_state = buddy_node_get(manager->nodes, right);
    uint8_t state = BUDDY_NODE_MIXED;
    uint32_t end = start + (UINT32_C(1) << order);
    if (left_state == BUDDY_NODE_BLOCKED &&
        right_state == BUDDY_NODE_BLOCKED) {
      state = BUDDY_NODE_BLOCKED;
    } else if (order <= PMM_BUDDY_MAX_ORDER &&
               left_state == BUDDY_NODE_FREE &&
               right_state == BUDDY_NODE_FREE &&
               !buddy_boundary_inside(manager, start, end)) {
      state = BUDDY_NODE_FREE;
      buddy_node_set(manager->nodes, left, BUDDY_NODE_BLOCKED);
      buddy_node_set(manager->nodes, right, BUDDY_NODE_BLOCKED);
    }
    buddy_node_set(manager->nodes, index, state);
  }

  manager->free_frames = 0;
  manager->allocated_frames = 0;
  for (uint32_t order = 0; order <= PMM_BUDDY_MAX_ORDER; ++order)
    manager->free_blocks[order] = 0;
  for (uint32_t index = 0; index < PMM_BUDDY_NODE_COUNT; ++index) {
    if (buddy_node_get(manager->nodes, index) != BUDDY_NODE_FREE)
      continue;
    uint32_t start = 0, order = 0;
    buddy_node_info(index, &start, &order);
    (void)start;
    if (order <= PMM_BUDDY_MAX_ORDER) {
      manager->free_blocks[order]++;
      manager->free_frames += UINT32_C(1) << order;
    }
  }
}

pmm_status pmm_buddy_init(pmm_buddy *manager, const lbp_view *view,
                          uint8_t *node_storage, uint64_t node_storage_size,
                          uint32_t *boundary_storage,
                          uint32_t boundary_capacity) {
  if (!manager || !view || !node_storage || !boundary_storage)
    return PMM_INVALID_ARGUMENT;
  if (node_storage_size < PMM_BUDDY_STATE_BYTES)
    return PMM_STORAGE_TOO_SMALL;

  pmm_table memory = {0}, reservations = {0};
  if (!find_table(view, LBP_MEMORY_MAP, &memory) ||
      !find_table(view, LBP_RESERVATIONS, &reservations) ||
      !validate_memory_table(&memory) || !validate_reservations(&reservations))
    return PMM_INVALID_MAP;
  uint64_t needed_boundaries = 0;
  for (uint64_t i = 0; i < memory.count; ++i) {
    const lbp_memory *entry =
        (const lbp_memory *)(memory.entries + i * memory.stride);
    if (entry->type == LBP_USABLE && entry->base < PMM_PHYSICAL_LIMIT)
      needed_boundaries += 2;
  }
  for (uint64_t i = 0; i < reservations.count; ++i) {
    const lbp_reservation *entry =
        (const lbp_reservation *)(reservations.entries + i * reservations.stride);
    if (entry->base < PMM_PHYSICAL_LIMIT)
      needed_boundaries += 2;
  }
  if (needed_boundaries > boundary_capacity)
    return PMM_STORAGE_TOO_SMALL;

  pmm_buddy initialized = {.nodes = node_storage,
                           .boundaries = boundary_storage,
                           .initialized = PMM_BUDDY_MAGIC};
  for (uint64_t i = 0; i < PMM_BUDDY_STATE_BYTES; ++i)
    node_storage[i] = 0xff;
  uint32_t boundary_count = 0;
  for (uint64_t i = 0; i < memory.count; ++i) {
    const lbp_memory *entry =
        (const lbp_memory *)(memory.entries + i * memory.stride);
    if (entry->type != LBP_USABLE)
      continue;
    uint64_t end = entry->base + entry->length;
    if (entry->base < PMM_PHYSICAL_LIMIT) {
      uint64_t clipped_end = end < PMM_PHYSICAL_LIMIT ? end : PMM_PHYSICAL_LIMIT;
      if (!buddy_add_boundary(boundary_storage, boundary_capacity,
                              &boundary_count, entry->base) ||
          !buddy_add_boundary(boundary_storage, boundary_capacity,
                              &boundary_count, clipped_end))
        return PMM_STORAGE_TOO_SMALL;
      buddy_mark_range(node_storage, entry->base, entry->length,
                       BUDDY_NODE_FREE);
    }
  }
  for (uint64_t i = 0; i < reservations.count; ++i) {
    const lbp_reservation *entry =
        (const lbp_reservation *)(reservations.entries + i * reservations.stride);
    if (entry->base >= PMM_PHYSICAL_LIMIT)
      continue;
    uint64_t end = entry->base + entry->length;
    uint64_t clipped_end = end < PMM_PHYSICAL_LIMIT ? end : PMM_PHYSICAL_LIMIT;
    if (!buddy_add_boundary(boundary_storage, boundary_capacity,
                            &boundary_count, entry->base) ||
        !buddy_add_boundary(boundary_storage, boundary_capacity,
                            &boundary_count, clipped_end))
      return PMM_STORAGE_TOO_SMALL;
    buddy_mark_range(node_storage, entry->base, entry->length,
                     BUDDY_NODE_BLOCKED);
  }
  buddy_sort_boundaries(boundary_storage, boundary_count);
  uint32_t unique_count = 0;
  for (uint32_t i = 0; i < boundary_count; ++i)
    if (!unique_count || boundary_storage[i] != boundary_storage[unique_count - 1u])
      boundary_storage[unique_count++] = boundary_storage[i];
  initialized.boundary_count = unique_count;
  buddy_build_tree(&initialized);
  *manager = initialized;
  return PMM_OK;
}

static int buddy_find_free(pmm_buddy *manager, uint32_t node, uint32_t start,
                           uint32_t order, uint32_t target_order,
                           uint32_t *frame) {
  uint8_t state = buddy_node_get(manager->nodes, node);
  if (state == BUDDY_NODE_BLOCKED || state == BUDDY_NODE_ALLOCATED)
    return 0;
  if (state == BUDDY_NODE_FREE) {
    if (order < target_order)
      return 0;
    if (order == target_order) {
      buddy_node_set(manager->nodes, node, BUDDY_NODE_ALLOCATED);
      *frame = start;
      return 1;
    }
    uint32_t left = node * 2u + 1u;
    uint32_t child_order = order - 1u;
    uint32_t half = UINT32_C(1) << child_order;
    manager->free_blocks[order]--;
    manager->free_blocks[child_order] += 2u;
    buddy_node_set(manager->nodes, node, BUDDY_NODE_MIXED);
    buddy_node_set(manager->nodes, left, BUDDY_NODE_FREE);
    buddy_node_set(manager->nodes, left + 1u, BUDDY_NODE_FREE);
    (void)half;
    state = BUDDY_NODE_MIXED;
  }
  if (order == 0)
    return 0;
  uint32_t left = node * 2u + 1u;
  uint32_t child_order = order - 1u;
  uint32_t half = UINT32_C(1) << child_order;
  if (buddy_find_free(manager, left, start, child_order, target_order, frame))
    return 1;
  return buddy_find_free(manager, left + 1u, start + half, child_order,
                         target_order, frame);
}

pmm_status pmm_buddy_alloc_order(pmm_buddy *manager, uint32_t order,
                                 uint64_t *physical) {
  if (!manager || !physical || manager->initialized != PMM_BUDDY_MAGIC ||
      !manager->nodes)
    return PMM_INVALID_ARGUMENT;
  if (order > PMM_BUDDY_MAX_ORDER)
    return PMM_INVALID_ARGUMENT;
  uint32_t frame = 0;
  if (!buddy_find_free(manager, 0, 0, 20, order, &frame))
    return PMM_OUT_OF_MEMORY;
  manager->free_blocks[order]--;
  uint32_t allocated = UINT32_C(1) << order;
  manager->free_frames -= allocated;
  manager->allocated_frames += allocated;
  *physical = (uint64_t)frame * PMM_PAGE_SIZE;
  return PMM_OK;
}

pmm_status pmm_buddy_free_order(pmm_buddy *manager, uint64_t physical,
                                uint32_t order) {
  if (!manager || manager->initialized != PMM_BUDDY_MAGIC ||
      !manager->nodes)
    return PMM_INVALID_ARGUMENT;
  if (order > PMM_BUDDY_MAX_ORDER)
    return PMM_INVALID_ARGUMENT;
  uint64_t bytes = PMM_PAGE_SIZE << order;
  if (physical & (bytes - 1u))
    return PMM_INVALID_ARGUMENT;
  if (physical >= PMM_PHYSICAL_LIMIT || bytes > PMM_PHYSICAL_LIMIT - physical)
    return PMM_OUT_OF_RANGE;

  uint32_t frame = (uint32_t)(physical / PMM_PAGE_SIZE);
  uint32_t node = 0, start = 0, node_order = 20;
  while (node_order > order) {
    uint8_t state = buddy_node_get(manager->nodes, node);
    if (state == BUDDY_NODE_FREE || state == BUDDY_NODE_BLOCKED ||
        state == BUDDY_NODE_ALLOCATED)
      return PMM_NOT_ALLOCATED;
    uint32_t half = UINT32_C(1) << (node_order - 1u);
    if (frame < start + half) {
      node = node * 2u + 1u;
    } else {
      node = node * 2u + 2u;
      start += half;
    }
    --node_order;
  }
  if (buddy_node_get(manager->nodes, node) != BUDDY_NODE_ALLOCATED ||
      start != frame)
    return PMM_NOT_ALLOCATED;

  uint32_t released = UINT32_C(1) << order;
  manager->allocated_frames -= released;
  manager->free_frames += released;
  buddy_node_set(manager->nodes, node, BUDDY_NODE_FREE);
  manager->free_blocks[order]++;
  while (node_order < PMM_BUDDY_MAX_ORDER) {
    uint32_t parent = (node - 1u) / 2u;
    uint32_t parent_start = 0, parent_order = 0;
    buddy_node_info(parent, &parent_start, &parent_order);
    uint32_t parent_end = parent_start + (UINT32_C(1) << parent_order);
    if (parent_order > PMM_BUDDY_MAX_ORDER ||
        buddy_boundary_inside(manager, parent_start, parent_end))
      break;
    uint32_t sibling = ((node & 1u) != 0) ? node + 1u : node - 1u;
    if (buddy_node_get(manager->nodes, sibling) != BUDDY_NODE_FREE)
      break;
    manager->free_blocks[node_order] -= 2u;
    manager->free_blocks[parent_order]++;
    buddy_node_set(manager->nodes, node, BUDDY_NODE_BLOCKED);
    buddy_node_set(manager->nodes, sibling, BUDDY_NODE_BLOCKED);
    buddy_node_set(manager->nodes, parent, BUDDY_NODE_FREE);
    node = parent;
    node_order = parent_order;
  }
  return PMM_OK;
}

pmm_status pmm_buddy_alloc_frame(pmm_buddy *manager, uint64_t *physical) {
  return pmm_buddy_alloc_order(manager, 0, physical);
}

pmm_status pmm_buddy_free_frame(pmm_buddy *manager, uint64_t physical) {
  return pmm_buddy_free_order(manager, physical, 0);
}

pmm_status pmm_buddy_get_stats(const pmm_buddy *manager,
                               pmm_buddy_stats *stats) {
  if (!manager || !stats || manager->initialized != PMM_BUDDY_MAGIC ||
      !manager->nodes)
    return PMM_INVALID_ARGUMENT;
  uint64_t largest = 0;
  for (int order = (int)PMM_BUDDY_MAX_ORDER; order >= 0; --order)
    if (manager->free_blocks[order]) {
      largest = (PMM_PAGE_SIZE << order);
      break;
    }
  *stats = (pmm_buddy_stats){.free_frames = manager->free_frames,
                             .allocated_frames = manager->allocated_frames,
                             .largest_free_block_bytes = largest};
  for (uint32_t order = 0; order <= PMM_BUDDY_MAX_ORDER; ++order)
    stats->free_blocks[order] = manager->free_blocks[order];
  return PMM_OK;
}
