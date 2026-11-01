/* SPDX-License-Identifier: GPL-3.0-only */
#include "block.h"

#include <assert.h>
#include <pthread.h>
#include <sched.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>

#define REQUESTS 2000u
#define PENDING 32u

typedef struct concurrent_device {
  atomic_flag lock;
  block_request *pending[PENDING];
  size_t count;
  atomic_uint submitted;
  atomic_uint completed;
  atomic_uint callback_count;
  block_device *device;
} concurrent_device;

static void acquire(concurrent_device *state) {
  while (atomic_flag_test_and_set_explicit(&state->lock, memory_order_acquire))
    sched_yield();
}
static void release(concurrent_device *state) {
  atomic_flag_clear_explicit(&state->lock, memory_order_release);
}
static block_status submit(block_device *device, block_request *request) {
  concurrent_device *state = device->driver;
  acquire(state);
  if (state->count == PENDING) {
    release(state);
    return BLOCK_BUSY;
  }
  state->pending[state->count++] = request;
  atomic_fetch_add_explicit(&state->submitted, 1, memory_order_relaxed);
  release(state);
  return BLOCK_OK;
}
static size_t poll(block_device *device, size_t budget) {
  concurrent_device *state = device->driver;
  size_t done = 0;
  while (done < budget) {
    acquire(state);
    if (!state->count) {
      release(state);
      break;
    }
    block_request *request = state->pending[--state->count];
    release(state);
    block_request_complete(request, BLOCK_OK);
    atomic_fetch_add_explicit(&state->completed, 1, memory_order_relaxed);
    ++done;
  }
  return done;
}
static block_status reset(block_device *device) {
  (void)device;
  return BLOCK_OK;
}
static void completion(block_request *request, void *context) {
  concurrent_device *state = context;
  assert(request->status == BLOCK_OK);
  atomic_fetch_add_explicit(&state->callback_count, 1, memory_order_relaxed);
}

typedef struct producer_args {
  block_device *device;
  concurrent_device *state;
  unsigned base;
} producer_args;

static void *produce(void *argument) {
  producer_args *args = argument;
  block_request *requests = calloc(REQUESTS, sizeof(*requests));
  uint8_t *buffers = calloc(REQUESTS, 512);
  assert(requests && buffers);
  for (unsigned i = 0; i < REQUESTS; ++i) {
    block_request_init(&requests[i], BLOCK_READ, args->base + i, 1,
                       buffers + (size_t)i * 512, completion, args->state);
    for (;;) {
      block_status status = block_submit(args->device, &requests[i]);
      if (status == BLOCK_OK) break;
      assert(status == BLOCK_BUSY);
      while (__atomic_load_n(&requests[i].state, __ATOMIC_ACQUIRE) != 0)
        sched_yield();
      sched_yield();
    }
  }
  for (unsigned i = 0; i < REQUESTS; ++i)
    while (__atomic_load_n(&requests[i].state, __ATOMIC_ACQUIRE) != 2)
      sched_yield();
  free(buffers);
  free(requests);
  return 0;
}

static void *poller(void *argument) {
  concurrent_device *state = argument;
  unsigned target = REQUESTS * 2;
  while (atomic_load_explicit(&state->completed, memory_order_acquire) < target) {
    (void)atomic_load_explicit(&state->submitted, memory_order_relaxed);
    if (state->device) (void)block_poll(state->device, PENDING);
    sched_yield();
  }
  return 0;
}

int main(void) {
  concurrent_device state = {0};
  atomic_flag_clear(&state.lock);
  static const block_device_ops ops = {submit, poll, reset};
  block_device device;
  assert(block_device_init(&device, &ops, &state, REQUESTS * 2, 512, 0) ==
         BLOCK_OK);
  state.device = &device;
  producer_args first = {&device, &state, 0};
  producer_args second = {&device, &state, REQUESTS};
  pthread_t consumer, p1, p2;
  assert(!pthread_create(&consumer, 0, poller, &state));
  assert(!pthread_create(&p1, 0, produce, &first));
  assert(!pthread_create(&p2, 0, produce, &second));
  assert(!pthread_join(p1, 0));
  assert(!pthread_join(p2, 0));
  assert(!pthread_join(consumer, 0));
  assert(atomic_load(&state.completed) == REQUESTS * 2);
  assert(atomic_load(&state.callback_count) == REQUESTS * 2);
  return 0;
}
