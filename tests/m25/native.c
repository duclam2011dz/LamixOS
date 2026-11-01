/* SPDX-License-Identifier: GPL-3.0-only */
#include "virtio_blk.h"

#include <assert.h>
#include <pthread.h>
#include <sched.h>
#include <stdint.h>
#include <stdatomic.h>
#include <string.h>

#define MOCK_STATUS_ACK 1u

typedef struct mock {
  uint32_t status;
  uint32_t device_feature_select;
  uint32_t driver_feature_select;
  uint32_t features_low;
  uint32_t features_high;
  uint16_t queue_size;
  uint16_t queue_max_size;
  uint16_t queue_select;
  uint16_t queue_enabled;
  uint16_t notify_offset;
  uint16_t last_available;
  uint64_t capacity;
  uint32_t block_size;
  uint8_t reset_stuck;
  uint8_t auto_complete;
  uint8_t reverse_pair;
  uint8_t fail_used_id;
  uint8_t corrupt_length;
  uint8_t device_status;
  uint16_t pending[32];
  uint16_t pending_count;
  uint32_t callbacks;
  uint32_t callback_errors;
  virtio_blk *driver;
} mock;

typedef struct mock_request_header {
  uint32_t type;
  uint32_t reserved;
  uint64_t sector;
} __attribute__((packed)) mock_request_header;

static uint8_t descriptors[4096] __attribute__((aligned(4096)));
static uint8_t available[4096] __attribute__((aligned(4096)));
static uint8_t used[4096] __attribute__((aligned(4096)));
static uint8_t headers[4096] __attribute__((aligned(4096)));
static uint8_t statuses[4096] __attribute__((aligned(4096)));
static uint8_t test_data[8][512] __attribute__((aligned(16)));

static void complete(mock *m, uint16_t head) {
  virtio_blk *d = m->driver;
  assert(head < VIRTIO_BLK_MAX_REQUESTS * 3u && !(head % 3u));
  virtio_blk_desc *table = d->descriptor_table;
  mock_request_header *header =
      (mock_request_header *)(uintptr_t)table[head].address;
  uint8_t *status = (uint8_t *)(uintptr_t)table[head + 2].address;
  *status = m->device_status;
  uint32_t length = 1;
  if (header->type == 0 && m->device_status == 0) {
    virtio_blk_desc *data_desc = &table[head + 1];
    uint8_t *data = (uint8_t *)(uintptr_t)data_desc->address;
    length += data_desc->length;
    for (uint32_t i = 0; i < data_desc->length; ++i)
      data[i] = (uint8_t)((header->sector + i) & 0xff);
  }
  uint16_t index = *d->used_index;
  d->used_ring[index % m->queue_size] = (virtio_blk_used_element){
      m->fail_used_id ? VIRTIO_BLK_QUEUE_SIZE : head,
      m->corrupt_length ? length + 1u : length};
  __atomic_thread_fence(__ATOMIC_RELEASE);
  *d->used_index = (uint16_t)(index + 1u);
}

static uint32_t read_common(void *context, uint16_t offset, unsigned width) {
  mock *m = context;
  if (offset == 20 && width == 1)
    return m->reset_stuck && m->status == 0 ? MOCK_STATUS_ACK : m->status;
  if (offset == 4 && width == 4)
    return m->device_feature_select ? m->features_high : m->features_low;
  if (offset == 24 && width == 2) return m->queue_max_size;
  if (offset == 28 && width == 2) return m->queue_enabled;
  if (offset == 30 && width == 2) return m->notify_offset;
  if (offset == 21 && width == 1) return 1;
  return 0;
}
static void write_common(void *context, uint16_t offset, unsigned width,
                         uint32_t value) {
  mock *m = context;
  if (offset == 20 && width == 1) {
    m->status = value;
    if (!value && !m->reset_stuck) m->status = 0;
  } else if (offset == 0 && width == 4) m->device_feature_select = value;
  else if (offset == 12 && width == 4) m->driver_feature_select = value;
  else if (offset == 24 && width == 2) m->queue_size = (uint16_t)value;
  else if (offset == 22 && width == 2) m->queue_select = (uint16_t)value;
  else if (offset == 28 && width == 2) m->queue_enabled = (uint16_t)value;
}
static uint32_t read_device(void *context, uint16_t offset, unsigned width) {
  mock *m = context;
  if (width != 4) return 0;
  if (offset == 0) return (uint32_t)m->capacity;
  if (offset == 4) return (uint32_t)(m->capacity >> 32);
  if (offset == 20) return m->block_size;
  return 0;
}
static void notify(void *context, uint16_t queue, uint16_t offset) {
  mock *m = context;
  (void)queue; (void)offset;
  virtio_blk *d = m->driver;
  uint16_t count = *d->available_index;
  while (m->last_available != count) {
    assert(m->queue_size != 0);
    uint16_t head = d->available_ring[m->last_available % m->queue_size];
    ++m->last_available;
    m->pending[m->pending_count++] = head;
  }
  if (m->auto_complete) {
    if (m->reverse_pair && m->pending_count >= 2) {
      complete(m, m->pending[1]);
      complete(m, m->pending[0]);
      m->pending_count = 0;
    } else {
      while (m->pending_count) {
        uint16_t head = m->pending[0];
        memmove(m->pending, m->pending + 1,
                (--m->pending_count) * sizeof(m->pending[0]));
        complete(m, head);
      }
    }
  }
}
static int dma_address(void *context, const void *pointer, size_t length,
                       uint64_t *physical) {
  (void)context;
  if (!pointer || !length || !physical ||
      (uintptr_t)pointer > UINT64_MAX - length) return 0;
  *physical = (uintptr_t)pointer;
  return 1;
}
static void lock(void *context) { (void)context; }
static void unlock(void *context) { (void)context; }
static void callback(block_request *request, void *context) {
  mock *m = context;
  ++m->callbacks;
  if (request->status != BLOCK_OK) ++m->callback_errors;
  if (__atomic_load_n(&request->state, __ATOMIC_ACQUIRE) != 3u)
    ++m->callback_errors;
}

static void reset_storage(void) {
  memset(descriptors, 0, sizeof(descriptors));
  memset(available, 0, sizeof(available));
  memset(used, 0, sizeof(used));
  memset(headers, 0, sizeof(headers));
  memset(statuses, 0, sizeof(statuses));
}
static block_status initialize_version(mock *m, virtio_blk *d, int flush,
                                       int read_only, int version_1,
                                       uint16_t maximum_queue_size) {
  reset_storage();
  *m = (mock){.features_low = (UINT32_C(1) << 6) |
      (flush ? UINT32_C(1) << 9 : 0) |
      (read_only ? UINT32_C(1) << 5 : 0),
      .features_high = version_1 ? UINT32_C(1) : 0,
      .queue_size = 0, .queue_max_size = maximum_queue_size, .capacity = 128,
      .block_size = 512, .auto_complete = 1, .reverse_pair = 1};
  virtio_blk_memory memory = {descriptors, sizeof(descriptors),
      (uintptr_t)descriptors, available, sizeof(available),
      (uintptr_t)available, used, sizeof(used), (uintptr_t)used,
      headers, sizeof(headers), statuses, sizeof(statuses)};
  virtio_blk_ops ops = {m, read_common, write_common, read_device, notify,
                        dma_address, lock, unlock, 0};
  block_status status = virtio_blk_initialize(d, &ops, &memory);
  m->driver = d;
  return status;
}

static block_status initialize(mock *m, virtio_blk *d, int flush,
                               int read_only) {
  return initialize_version(m, d, flush, read_only, 1, 32);
}

static void test_features_and_out_of_order_completion(void) {
  mock m;
  virtio_blk d;
  assert(initialize(&m, &d, 1, 0) == BLOCK_OK);
  block_request a, b;
  block_request_init(&a, BLOCK_WRITE, 8, 1, test_data[0], callback, &m);
  block_request_init(&b, BLOCK_WRITE, 10, 1, test_data[1], callback, &m);
  assert(block_submit(&d.device, &a) == BLOCK_OK);
  assert(block_submit(&d.device, &b) == BLOCK_OK);
  assert(m.callbacks == 0);
  assert(block_poll(&d.device, 8) == 2);
  assert(m.callbacks == 2 && !m.callback_errors);
  assert(a.state == 2 && b.state == 2);
  block_request_complete(&a, BLOCK_DEVICE_ERROR);
  assert(m.callbacks == 2 && a.status == BLOCK_OK);
  block_request flush;
  block_request_init(&flush, BLOCK_FLUSH, 0, 0, 0, callback, &m);
  assert(block_io_sync(&d.device, &flush, 32) == BLOCK_OK);
  assert(m.callbacks == 3);
}

static void test_queue_reduction_and_wraparound(void) {
  mock m;
  virtio_blk d;
  assert(initialize_version(&m, &d, 1, 0, 1, 256) == BLOCK_OK);
  assert(d.queue_size == VIRTIO_BLK_QUEUE_SIZE);
  assert(m.queue_size == VIRTIO_BLK_QUEUE_SIZE);
  for (uint64_t i = 0; i < 40; ++i) {
    block_request request;
    block_request_init(&request, BLOCK_WRITE, i, 1, test_data[i % 8],
                       callback, &m);
    assert(block_io_sync(&d.device, &request, 1) == BLOCK_OK);
    assert(__atomic_load_n(&request.state, __ATOMIC_ACQUIRE) == 2u);
    assert(d.initialized && !d.offline);
  }
  assert(m.callbacks == 40 && !m.callback_errors);
}

static void test_sync_completion_at_budget_boundary(void) {
  mock m;
  virtio_blk d;
  assert(initialize(&m, &d, 1, 0) == BLOCK_OK);
  block_request request;
  block_request_init(&request, BLOCK_READ, 1, 1, test_data[0], callback, &m);
  assert(block_io_sync(&d.device, &request, 1) == BLOCK_OK);
  assert(d.initialized && d.accepting && !d.offline);
  assert(m.callbacks == 1 && !m.callback_errors);
}

static void test_read_error_short_used_length(void) {
  mock m;
  virtio_blk d;
  assert(initialize(&m, &d, 1, 0) == BLOCK_OK);
  m.device_status = 1;
  block_request request;
  block_request_init(&request, BLOCK_READ, 5, 1, test_data[0], callback, &m);
  assert(block_submit(&d.device, &request) == BLOCK_OK);
  assert(block_poll(&d.device, 1) == 1);
  assert(request.state == 2 && request.status == BLOCK_DEVICE_ERROR);
  assert(d.initialized && !d.offline);
  assert(m.callbacks == 1 && m.callback_errors == 1);
}

typedef struct completion_race {
  block_request *request;
  atomic_int callback_entered;
  atomic_int callback_release;
  atomic_int callback_returned;
  atomic_int reset_returned;
} completion_race;

static block_status race_submit(block_device *device, block_request *request) {
  completion_race *race = device->driver;
  race->request = request;
  return BLOCK_OK;
}

static size_t race_poll(block_device *device, size_t budget) {
  (void)device;
  (void)budget;
  return 0;
}

static void race_callback(block_request *request, void *context) {
  completion_race *race = context;
  assert(request->status == BLOCK_OK);
  atomic_store_explicit(&race->callback_entered, 1, memory_order_release);
  while (!atomic_load_explicit(&race->callback_release, memory_order_acquire))
    sched_yield();
  atomic_store_explicit(&race->callback_returned, 1, memory_order_release);
}

static void *race_finish(void *context) {
  completion_race *race = context;
  block_request_complete(race->request, BLOCK_OK);
  return 0;
}

static void *race_release_callback(void *context) {
  completion_race *race = context;
  while (!atomic_load_explicit(&race->reset_returned, memory_order_acquire))
    sched_yield();
  for (unsigned i = 0; i < 10000; ++i) sched_yield();
  atomic_store_explicit(&race->callback_release, 1, memory_order_release);
  return 0;
}

static block_status race_reset(block_device *device) {
  completion_race *race = device->driver;
  pthread_t finisher;
  assert(!pthread_create(&finisher, 0, race_finish, race));
  while (!atomic_load_explicit(&race->callback_entered, memory_order_acquire))
    sched_yield();
  atomic_store_explicit(&race->reset_returned, 1, memory_order_release);
  assert(!pthread_detach(finisher));
  return BLOCK_OK;
}

static void test_sync_waits_for_completion_callback(void) {
  completion_race race = {0};
  static const block_device_ops ops = {race_submit, race_poll, race_reset};
  block_device device;
  assert(block_device_init(&device, &ops, &race, 32, 512, 0) == BLOCK_OK);
  pthread_t releaser;
  assert(!pthread_create(&releaser, 0, race_release_callback, &race));
  uint8_t buffer[512] = {0};
  block_request request;
  block_request_init(&request, BLOCK_READ, 0, 1, buffer, race_callback, &race);
  assert(block_io_sync(&device, &request, 1) == BLOCK_OK);
  assert(__atomic_load_n(&request.state, __ATOMIC_ACQUIRE) == 2u);
  assert(atomic_load_explicit(&race.callback_returned, memory_order_acquire));
  assert(!pthread_join(releaser, 0));
}

static void test_validation_and_read_only(void) {
  mock m;
  virtio_blk d;
  assert(initialize(&m, &d, 1, 1) == BLOCK_OK);
  block_request request;
  block_request_init(&request, BLOCK_WRITE, 0, 1, test_data[0], 0, 0);
  assert(block_submit(&d.device, &request) == BLOCK_READ_ONLY);
  block_request_init(&request, BLOCK_READ, 128, 1, test_data[0], 0, 0);
  assert(block_submit(&d.device, &request) == BLOCK_RANGE);
  block_request_init(&request, BLOCK_READ, UINT64_MAX, 1, test_data[0], 0, 0);
  assert(block_submit(&d.device, &request) == BLOCK_RANGE);
  block_request_init(&request, (block_operation)99, 0, 1,
                     test_data[0], 0, 0);
  assert(block_submit(&d.device, &request) == BLOCK_INVALID);
  assert(initialize(&m, &d, 0, 0) == BLOCK_OK);
  block_request_init(&request, BLOCK_FLUSH, 0, 0, 0, 0, 0);
  assert(block_io_sync(&d.device, &request, 10) == BLOCK_UNSUPPORTED);
  assert(initialize_version(&m, &d, 1, 0, 0, 32) == BLOCK_UNSUPPORTED);
}

typedef struct reentrant_context {
  mock *device_mock;
  block_device *device;
  block_request nested;
  uint8_t nested_buffer[512];
  uint32_t calls;
} reentrant_context;

static void reentrant_callback(block_request *request, void *context) {
  reentrant_context *state = context;
  assert(request->status == BLOCK_OK);
  ++state->calls;
  block_request_init(&state->nested, BLOCK_WRITE, 22, 1,
                     state->nested_buffer, 0, 0);
  assert(block_submit(state->device, &state->nested) == BLOCK_OK);
}

static void test_callback_can_submit_async(void) {
  mock m;
  virtio_blk d;
  assert(initialize(&m, &d, 1, 0) == BLOCK_OK);
  m.reverse_pair = 0;
  reentrant_context context = {&m, &d.device, {0}, {0}, 0};
  block_request first;
  block_request_init(&first, BLOCK_WRITE, 21, 1, test_data[0],
                     reentrant_callback, &context);
  assert(block_submit(&d.device, &first) == BLOCK_OK);
  assert(block_poll(&d.device, 4) == 1);
  assert(block_poll(&d.device, 4) == 1);
  assert(context.calls == 1 && context.nested.state == 2);
}

static void test_queue_exhaustion_and_reset_lifetimes(void) {
  mock m;
  virtio_blk d;
  assert(initialize(&m, &d, 1, 0) == BLOCK_OK);
  m.auto_complete = 0;
  block_request requests[VIRTIO_BLK_MAX_REQUESTS + 1];
  for (size_t i = 0; i < VIRTIO_BLK_MAX_REQUESTS; ++i) {
    block_request_init(&requests[i], BLOCK_WRITE, i, 1, test_data[i],
                       callback, &m);
    assert(block_submit(&d.device, &requests[i]) == BLOCK_OK);
  }
  block_request_init(&requests[VIRTIO_BLK_MAX_REQUESTS], BLOCK_WRITE,
      20, 1, test_data[0], callback, &m);
  assert(block_submit(&d.device, &requests[VIRTIO_BLK_MAX_REQUESTS]) ==
         BLOCK_BUSY);
  assert(m.callbacks == 0);

  assert(initialize(&m, &d, 1, 0) == BLOCK_OK);
  m.auto_complete = 0;
  m.reset_stuck = 1;
  block_request stuck;
  block_request_init(&stuck, BLOCK_WRITE, 3, 1, test_data[0], callback, &m);
  assert(block_io_sync(&d.device, &stuck, 1) == BLOCK_RESET_TIMEOUT);
  assert(stuck.state == 1 && m.callbacks == 0);

  assert(initialize(&m, &d, 1, 0) == BLOCK_OK);
  m.auto_complete = 0;
  m.reset_stuck = 0;
  block_request request;
  block_request_init(&request, BLOCK_WRITE, 4, 1, test_data[0], callback, &m);
  assert(block_io_sync(&d.device, &request, 1) == BLOCK_TIMEOUT);
  assert(request.state == 2 && request.status == BLOCK_TIMEOUT);
  assert(m.callbacks == 1);
}

static void test_malformed_used_id_fails_closed(void) {
  mock m;
  virtio_blk d;
  assert(initialize(&m, &d, 1, 0) == BLOCK_OK);
  m.fail_used_id = 1;
  block_request request;
  block_request_init(&request, BLOCK_WRITE, 1, 1, test_data[0], callback, &m);
  assert(block_submit(&d.device, &request) == BLOCK_OK);
  assert(block_poll(&d.device, 1) == 0);
  assert(d.offline && request.state == 2 && request.status == BLOCK_TIMEOUT);
  assert(m.callbacks == 1 && m.callback_errors == 1);

  assert(initialize(&m, &d, 1, 0) == BLOCK_OK);
  m.corrupt_length = 1;
  block_request_init(&request, BLOCK_WRITE, 1, 1, test_data[0], callback, &m);
  assert(block_submit(&d.device, &request) == BLOCK_OK);
  assert(block_poll(&d.device, 1) == 0);
  assert(d.offline && request.state == 2 && request.status == BLOCK_TIMEOUT);
  assert(m.callbacks == 1 && m.callback_errors == 1);
}

int main(void) {
  test_features_and_out_of_order_completion();
  test_queue_reduction_and_wraparound();
  test_sync_completion_at_budget_boundary();
  test_read_error_short_used_length();
  test_sync_waits_for_completion_callback();
  test_validation_and_read_only();
  test_callback_can_submit_async();
  test_queue_exhaustion_and_reset_lifetimes();
  test_malformed_used_id_fails_closed();
  return 0;
}
