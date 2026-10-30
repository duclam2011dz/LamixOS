/* SPDX-License-Identifier: GPL-3.0-only */
#define _POSIX_C_SOURCE 200809L
#include "pmm.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define BENCH_OPS 20000u
#define BENCH_SLOTS 512u
#define BENCH_REPETITIONS 5u
#define FRAGMENT_FRAMES 4096u

typedef struct bench_op {
  uint16_t slot;
  uint8_t order;
  uint8_t allocate;
} bench_op;

typedef struct sample {
  uint64_t init_ns;
  uint64_t elapsed_ns;
  uint64_t *latencies;
  uint64_t max_latency;
  uint64_t successes;
} sample;

static uint8_t bitmap_storage[PMM_STATE_BYTES];
static uint8_t buddy_nodes[PMM_BUDDY_STATE_BYTES];
static uint32_t buddy_boundaries[PMM_BUDDY_BOUNDARY_CAPACITY];
static uint64_t bitmap_frames[BENCH_SLOTS];
static uint64_t buddy_frames[BENCH_SLOTS];
static uint8_t bitmap_orders_live[BENCH_SLOTS];
static uint8_t buddy_orders_live[BENCH_SLOTS];
static uint8_t bitmap_live[BENCH_SLOTS];
static uint8_t buddy_live[BENCH_SLOTS];

static uint64_t now_ns(void) {
  struct timespec value;
  if (clock_gettime(CLOCK_MONOTONIC, &value) != 0)
    return 0;
  return (uint64_t)value.tv_sec * UINT64_C(1000000000) +
         (uint64_t)value.tv_nsec;
}

static int read_view(const char *path, uint8_t **blob, size_t *size,
                     lbp_view *view) {
  FILE *file = fopen(path, "rb");
  if (!file || fseek(file, 0, SEEK_END) != 0)
    return 0;
  long length = ftell(file);
  if (length <= 0 || fseek(file, 0, SEEK_SET) != 0)
    return 0;
  *blob = malloc((size_t)length);
  if (!*blob)
    return 0;
  *size = fread(*blob, 1, (size_t)length, file);
  return *size == (size_t)length && !ferror(file) && fclose(file) == 0 &&
         lbp_validate(*blob, *size, NULL, 0, view) == LBP_OK;
}

static uint8_t bitmap_state(const pmm *manager, uint32_t frame) {
  uint32_t shift = (frame & 3u) * 2u;
  return (uint8_t)((manager->states[frame >> 2] >> shift) & 3u);
}

static void bitmap_set_state(pmm *manager, uint32_t frame, uint8_t state) {
  uint32_t shift = (frame & 3u) * 2u;
  uint8_t mask = (uint8_t)(3u << shift);
  manager->states[frame >> 2] =
      (uint8_t)((manager->states[frame >> 2] & (uint8_t)~mask) |
                (uint8_t)(state << shift));
}

static pmm_status bitmap_alloc_order(pmm *manager, uint32_t order,
                                     uint64_t *physical) {
  if (order > PMM_BUDDY_MAX_ORDER)
    return PMM_INVALID_ARGUMENT;
  uint32_t count = UINT32_C(1) << order;
  for (uint32_t first = 0; first <= PMM_FRAME_COUNT - count; first += count) {
    uint32_t i = 0;
    for (; i < count && bitmap_state(manager, first + i) == 1u; ++i) {
    }
    if (i != count)
      continue;
    for (i = 0; i < count; ++i)
      bitmap_set_state(manager, first + i, 2u);
    manager->free_frames -= count;
    manager->allocated_frames += count;
    *physical = (uint64_t)first * PMM_PAGE_SIZE;
    return PMM_OK;
  }
  return PMM_OUT_OF_MEMORY;
}

static pmm_status bitmap_free_order(pmm *manager, uint64_t physical,
                                    uint32_t order) {
  if (order > PMM_BUDDY_MAX_ORDER)
    return PMM_INVALID_ARGUMENT;
  uint32_t count = UINT32_C(1) << order;
  if (physical & (((uint64_t)count * PMM_PAGE_SIZE) - 1u))
    return PMM_INVALID_ARGUMENT;
  if (physical >= PMM_PHYSICAL_LIMIT ||
      (uint64_t)count * PMM_PAGE_SIZE > PMM_PHYSICAL_LIMIT - physical)
    return PMM_OUT_OF_RANGE;
  uint32_t first = (uint32_t)(physical / PMM_PAGE_SIZE);
  for (uint32_t i = 0; i < count; ++i)
    if (bitmap_state(manager, first + i) != 2u)
      return PMM_NOT_ALLOCATED;
  for (uint32_t i = 0; i < count; ++i)
    bitmap_set_state(manager, first + i, 1u);
  manager->free_frames += count;
  manager->allocated_frames -= count;
  return PMM_OK;
}

static uint64_t bitmap_largest_free(pmm *manager, uint32_t *order_counts) {
  uint64_t largest = 0;
  memset(order_counts, 0,
         (PMM_BUDDY_MAX_ORDER + 1u) * sizeof(*order_counts));
  for (uint32_t order = 0; order <= PMM_BUDDY_MAX_ORDER; ++order) {
    uint32_t count = UINT32_C(1) << order;
    for (uint32_t first = 0; first <= PMM_FRAME_COUNT - count;
         first += count) {
      uint32_t i = 0;
      for (; i < count && bitmap_state(manager, first + i) == 1u; ++i) {
      }
      if (i == count) {
        order_counts[order]++;
        uint64_t bytes = (uint64_t)count * PMM_PAGE_SIZE;
        if (bytes > largest)
          largest = bytes;
      }
    }
  }
  return largest;
}

static int compare_u64(const void *left, const void *right) {
  uint64_t a = *(const uint64_t *)left, b = *(const uint64_t *)right;
  return (a > b) - (a < b);
}

static uint64_t percentile(uint64_t *values, uint32_t count, uint32_t pct) {
  qsort(values, count, sizeof(*values), compare_u64);
  uint32_t index = ((count - 1u) * pct + 99u) / 100u;
  return values[index];
}

static uint32_t next_random(uint32_t *state) {
  *state = *state * UINT32_C(1664525) + UINT32_C(1013904223);
  return *state;
}

static int run_trace(const lbp_view *view, const bench_op *trace,
                     uint64_t *trace_hash, sample *bitmap_result,
                     sample *buddy_result) {
  pmm bitmap = {0};
  pmm_buddy buddy = {0};
  uint64_t init_start = now_ns();
  if (pmm_init(&bitmap, view, bitmap_storage, sizeof(bitmap_storage)) != PMM_OK)
    return 0;
  bitmap_result->init_ns = now_ns() - init_start;
  init_start = now_ns();
  if (pmm_buddy_init(&buddy, view, buddy_nodes, sizeof(buddy_nodes),
                     buddy_boundaries, PMM_BUDDY_BOUNDARY_CAPACITY) != PMM_OK)
    return 0;
  buddy_result->init_ns = now_ns() - init_start;
  memset(bitmap_live, 0, sizeof(bitmap_live));
  memset(buddy_live, 0, sizeof(buddy_live));
  memset(bitmap_orders_live, 0, sizeof(bitmap_orders_live));
  memset(buddy_orders_live, 0, sizeof(buddy_orders_live));
  uint64_t start = now_ns();
  for (uint32_t i = 0; i < BENCH_OPS; ++i) {
    uint32_t slot = trace[i].slot;
    uint32_t order = trace[i].order;
    uint64_t op_start = now_ns();
    if (trace[i].allocate && !bitmap_live[slot]) {
      if (bitmap_alloc_order(&bitmap, order, &bitmap_frames[slot]) == PMM_OK) {
        bitmap_live[slot] = 1;
        bitmap_orders_live[slot] = (uint8_t)order;
        bitmap_result->successes++;
      }
    } else if (!trace[i].allocate && bitmap_live[slot]) {
      if (bitmap_free_order(&bitmap, bitmap_frames[slot], bitmap_orders_live[slot]) != PMM_OK)
        return 0;
      bitmap_live[slot] = 0;
    }
    bitmap_result->latencies[i] = now_ns() - op_start;
    if (bitmap_result->latencies[i] > bitmap_result->max_latency)
      bitmap_result->max_latency = bitmap_result->latencies[i];
  }
  bitmap_result->elapsed_ns = now_ns() - start;

  start = now_ns();
  for (uint32_t i = 0; i < BENCH_OPS; ++i) {
    uint32_t slot = trace[i].slot;
    uint32_t order = trace[i].order;
    uint64_t op_start = now_ns();
    if (trace[i].allocate && !buddy_live[slot]) {
      if (pmm_buddy_alloc_order(&buddy, order, &buddy_frames[slot]) == PMM_OK) {
        buddy_live[slot] = 1;
        buddy_orders_live[slot] = (uint8_t)order;
        buddy_result->successes++;
      }
    } else if (!trace[i].allocate && buddy_live[slot]) {
      if (pmm_buddy_free_order(&buddy, buddy_frames[slot], buddy_orders_live[slot]) != PMM_OK)
        return 0;
      buddy_live[slot] = 0;
    }
    buddy_result->latencies[i] = now_ns() - op_start;
    if (buddy_result->latencies[i] > buddy_result->max_latency)
      buddy_result->max_latency = buddy_result->latencies[i];
  }
  buddy_result->elapsed_ns = now_ns() - start;
  (void)trace_hash;
  return 1;
}

static int fragmentation(const lbp_view *view, uint32_t *bitmap_free,
                         uint64_t *bitmap_largest, uint32_t *bitmap_orders,
                         uint32_t *buddy_free, uint64_t *buddy_largest,
                         uint32_t *buddy_orders, int *bitmap_order1_fail,
                         int *buddy_order1_fail) {
  pmm bitmap = {0};
  pmm_buddy buddy = {0};
  if (pmm_init(&bitmap, view, bitmap_storage, sizeof(bitmap_storage)) != PMM_OK ||
      pmm_buddy_init(&buddy, view, buddy_nodes, sizeof(buddy_nodes),
                     buddy_boundaries, PMM_BUDDY_BOUNDARY_CAPACITY) != PMM_OK)
    return 0;
  uint64_t *bitmap_all = malloc(FRAGMENT_FRAMES * sizeof(*bitmap_all));
  uint64_t *buddy_all = malloc(FRAGMENT_FRAMES * sizeof(*buddy_all));
  if (!bitmap_all || !buddy_all)
    return 0;
  for (uint32_t i = 0; i < FRAGMENT_FRAMES; ++i)
    if (pmm_alloc_frame(&bitmap, &bitmap_all[i]) != PMM_OK ||
        pmm_buddy_alloc_frame(&buddy, &buddy_all[i]) != PMM_OK)
      return 0;
  for (uint32_t i = 1; i < FRAGMENT_FRAMES; i += 2u)
    if (bitmap_free_order(&bitmap, bitmap_all[i], 0) != PMM_OK ||
        pmm_buddy_free_frame(&buddy, buddy_all[i]) != PMM_OK)
      return 0;
  *bitmap_free = bitmap.free_frames;
  *bitmap_largest = bitmap_largest_free(&bitmap, bitmap_orders);
  pmm_buddy_stats stats;
  if (pmm_buddy_get_stats(&buddy, &stats) != PMM_OK)
    return 0;
  *buddy_free = stats.free_frames;
  *buddy_largest = stats.largest_free_block_bytes;
  memcpy(buddy_orders, stats.free_blocks,
         (PMM_BUDDY_MAX_ORDER + 1u) * sizeof(*buddy_orders));
  uint64_t address = 0;
  *bitmap_order1_fail = bitmap_alloc_order(&bitmap, 1, &address) ==
                        PMM_OUT_OF_MEMORY;
  *buddy_order1_fail = pmm_buddy_alloc_order(&buddy, 1, &address) ==
                       PMM_OUT_OF_MEMORY;
  free(bitmap_all);
  free(buddy_all);
  return 1;
}

int main(int argc, char **argv) {
  if (argc != 2)
    return 2;
  uint8_t *blob = NULL;
  size_t size = 0;
  lbp_view view = {0};
  if (!read_view(argv[1], &blob, &size, &view))
    return 3;
  bench_op *trace = calloc(BENCH_OPS, sizeof(*trace));
  if (!trace)
    return 4;
  uint32_t random = UINT32_C(0x12a110c);
  for (uint32_t i = 0; i < BENCH_OPS; ++i) {
    uint32_t value = next_random(&random);
    trace[i].slot = (uint16_t)((value >> 8) & (BENCH_SLOTS - 1u));
    trace[i].order = (uint8_t)((value >> 5) & 3u);
    trace[i].allocate = (uint8_t)((value >> 31) & 1u);
  }
  sample bitmap_runs[BENCH_REPETITIONS] = {0};
  sample buddy_runs[BENCH_REPETITIONS] = {0};
  uint64_t *bitmap_values = calloc(BENCH_OPS * BENCH_REPETITIONS,
                                   sizeof(uint64_t));
  uint64_t *buddy_values = calloc(BENCH_OPS * BENCH_REPETITIONS,
                                  sizeof(uint64_t));
  if (!bitmap_values || !buddy_values)
    return 5;
  uint64_t trace_hash = 0;
  trace_hash = UINT64_C(1469598103934665603);
  for (uint32_t i = 0; i < BENCH_OPS; ++i) {
    trace_hash = (trace_hash ^ trace[i].slot) * UINT64_C(1099511628211);
    trace_hash = (trace_hash ^ trace[i].order) * UINT64_C(1099511628211);
    trace_hash = (trace_hash ^ trace[i].allocate) * UINT64_C(1099511628211);
  }
  for (uint32_t rep = 0; rep < BENCH_REPETITIONS; ++rep) {
    bitmap_runs[rep].latencies = bitmap_values + (size_t)rep * BENCH_OPS;
    buddy_runs[rep].latencies = buddy_values + (size_t)rep * BENCH_OPS;
    if (!run_trace(&view, trace, &trace_hash, &bitmap_runs[rep],
                   &buddy_runs[rep]))
      return 6;
  }

  uint32_t bitmap_free = 0, buddy_free = 0;
  uint64_t bitmap_largest = 0, buddy_largest = 0;
  uint32_t bitmap_orders[PMM_BUDDY_MAX_ORDER + 1u] = {0};
  uint32_t buddy_orders[PMM_BUDDY_MAX_ORDER + 1u] = {0};
  int bitmap_order1_fail = 0, buddy_order1_fail = 0;
  if (!fragmentation(&view, &bitmap_free, &bitmap_largest, bitmap_orders,
                     &buddy_free, &buddy_largest, buddy_orders,
                     &bitmap_order1_fail, &buddy_order1_fail))
    return 7;

  uint64_t bitmap_total_ns = 0, buddy_total_ns = 0;
  uint64_t bitmap_init_total_ns = 0, buddy_init_total_ns = 0;
  uint64_t bitmap_max = 0, buddy_max = 0;
  for (uint32_t rep = 0; rep < BENCH_REPETITIONS; ++rep) {
    bitmap_total_ns += bitmap_runs[rep].elapsed_ns;
    buddy_total_ns += buddy_runs[rep].elapsed_ns;
    bitmap_init_total_ns += bitmap_runs[rep].init_ns;
    buddy_init_total_ns += buddy_runs[rep].init_ns;
    if (bitmap_runs[rep].max_latency > bitmap_max)
      bitmap_max = bitmap_runs[rep].max_latency;
    if (buddy_runs[rep].max_latency > buddy_max)
      buddy_max = buddy_runs[rep].max_latency;
  }
  uint64_t operations = (uint64_t)BENCH_OPS * BENCH_REPETITIONS;
  uint64_t bitmap_p50 = percentile(bitmap_values, (uint32_t)operations, 50);
  uint64_t bitmap_p95 = percentile(bitmap_values, (uint32_t)operations, 95);
  uint64_t buddy_p50 = percentile(buddy_values, (uint32_t)operations, 50);
  uint64_t buddy_p95 = percentile(buddy_values, (uint32_t)operations, 95);
  uint64_t bitmap_metadata = PMM_STATE_BYTES;
  uint64_t buddy_metadata = PMM_BUDDY_STATE_BYTES +
      PMM_BUDDY_BOUNDARY_CAPACITY * sizeof(uint32_t);
  printf("{\"seed\":\"0x12a110c\",\"trace_hash\":\"%016" PRIx64
         "\",\"operations_per_allocator\":%" PRIu64
         ",\"repetitions\":%u,\"metadata_bytes\":{\"bitmap\":%" PRIu64
         ",\"buddy_reserved\":%" PRIu64
         "},\"latency_ns_all_ops\":{\"bitmap_p50\":%" PRIu64
         ",\"bitmap_p95\":%" PRIu64 ",\"bitmap_max\":%" PRIu64
         ",\"buddy_p50\":%" PRIu64 ",\"buddy_p95\":%" PRIu64
         ",\"buddy_max\":%" PRIu64
         "},\"mean_init_ns\":{\"bitmap\":%.2f,\"buddy\":%.2f"
         "},\"mean_throughput_ops_s\":{\"bitmap\":%.2f,\"buddy\":%.2f"
         "},\"fragmentation\":{\"bitmap_free_frames\":%u"
         ",\"bitmap_largest_free_bytes\":%" PRIu64
         ",\"bitmap_order1_failed_with_free_frames\":%s"
         ",\"buddy_free_frames\":%u,\"buddy_largest_free_bytes\":%" PRIu64
         ",\"buddy_order1_failed_with_free_frames\":%s"
         "},\"bitmap_free_blocks_by_order\":[",
         trace_hash, operations, BENCH_REPETITIONS, bitmap_metadata,
         buddy_metadata, bitmap_p50, bitmap_p95, bitmap_max, buddy_p50,
         buddy_p95, buddy_max,
         (double)bitmap_init_total_ns / BENCH_REPETITIONS,
         (double)buddy_init_total_ns / BENCH_REPETITIONS,
         (double)operations * 1e9 / (double)bitmap_total_ns,
         (double)operations * 1e9 / (double)buddy_total_ns, bitmap_free,
         bitmap_largest, bitmap_order1_fail ? "true" : "false", buddy_free,
         buddy_largest, buddy_order1_fail ? "true" : "false");
  for (uint32_t order = 0; order <= PMM_BUDDY_MAX_ORDER; ++order)
    printf("%s%u", order ? "," : "", bitmap_orders[order]);
  printf("],\"buddy_free_blocks_by_order\":[");
  for (uint32_t order = 0; order <= PMM_BUDDY_MAX_ORDER; ++order)
    printf("%s%u", order ? "," : "", buddy_orders[order]);
  printf("],\"note\":\"host benchmark only; not guest performance\"}\n");
  free(bitmap_values);
  free(buddy_values);
  free(trace);
  free(blob);
  return 0;
}
