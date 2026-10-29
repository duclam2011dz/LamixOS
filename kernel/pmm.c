/* SPDX-License-Identifier: GPL-3.0-only */
#include "pmm.h"

#define STATE_UNAVAILABLE 0u
#define STATE_FREE 1u
#define STATE_ALLOCATED 2u
#define TABLE_PREFIX 32u
#define TABLE_LIMIT UINT64_C(32768)
#define PMM_MAGIC UINT32_C(0x504d4d39)

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
