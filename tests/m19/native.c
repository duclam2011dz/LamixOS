/* SPDX-License-Identifier: GPL-3.0-only */
#include "scheduler.h"

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct host_context {
  scheduler *state;
  int irq_enabled;
  size_t allocations;
  size_t fail_after;
  scheduler_frame frame;
} host_context;

static void *allocate(void *context, size_t size) {
  host_context *host = context;
  if (host->fail_after && host->allocations >= host->fail_after) return 0;
  void *memory = calloc(1, size);
  if (memory) ++host->allocations;
  return memory;
}
static void deallocate(void *context, void *pointer) {
  host_context *host = context;
  if (pointer) { --host->allocations; free(pointer); }
}
static uintptr_t irq_save(void *context) {
  host_context *host = context;
  uintptr_t was_enabled = (uintptr_t)host->irq_enabled;
  host->irq_enabled = 0;
  return was_enabled;
}
static void irq_restore(void *context, uintptr_t flags) {
  ((host_context *)context)->irq_enabled = (int)flags;
}
static int reschedule(void *context) {
  host_context *host = context;
  (void)scheduler_on_reschedule(host->state, &host->frame);
  return 0;
}
static void fatal(void *context, uint64_t reason) {
  (void)context; (void)reason; abort();
}
static void noop(void *argument) { (void)argument; }
static void idle(void *argument) { (void)argument; }
static void setup(host_context *host, scheduler *state) {
  memset(host, 0, sizeof(*host));
  host->state = state;
  host->irq_enabled = 1;
  scheduler_operations ops = {host, allocate, deallocate, irq_save,
                               irq_restore, reschedule, fatal, 0};
  assert(scheduler_init(state, &ops, idle) == SCHED_OK);
}

static void test_priority_rr(void) {
  host_context host;
  scheduler state;
  scheduler_thread *a, *b, *low;
  setup(&host, &state);
  assert(scheduler_create(&state, noop, 0, 3, &a) == SCHED_OK);
  assert(scheduler_create(&state, noop, 0, 3, &b) == SCHED_OK);
  assert(scheduler_create(&state, noop, 0, 0, &low) == SCHED_OK);
  assert(scheduler_begin(&state) == SCHED_OK);
  (void)scheduler_on_tick(&state, &host.frame);
  assert(state.current == a);
  (void)scheduler_on_tick(&state, &host.frame);
  assert(state.current == b);
  (void)scheduler_on_tick(&state, &host.frame);
  assert(state.current == a);
  assert(low->state == SCHED_THREAD_READY);
}

static void test_aging(void) {
  host_context host;
  scheduler state;
  scheduler_thread *low;
  setup(&host, &state);
  assert(scheduler_create(&state, noop, 0, 0, &low) == SCHED_OK);
  assert(scheduler_set_priority(state.bootstrap, 1) == SCHED_OK);
  assert(scheduler_begin(&state) == SCHED_OK);
  for (unsigned tick = 0; tick < SCHED_AGE_TICKS; ++tick)
    (void)scheduler_on_tick(&state, &host.frame);
  assert(state.current == low);
  assert(low->effective_priority == 0);
  assert(low->age_priority == 0);
  assert(scheduler_tick_count(&state) == SCHED_AGE_TICKS);
}

static void test_mutex_handoff_and_donation(void) {
  host_context host;
  scheduler state;
  scheduler_thread *owner, *waiter;
  scheduler_mutex mutex, unused;
  setup(&host, &state);
  scheduler_mutex_init(&mutex);
  scheduler_mutex_init(&unused);
  assert(scheduler_create(&state, noop, 0, 2, &owner) == SCHED_OK);
  assert(scheduler_begin(&state) == SCHED_OK);
  (void)scheduler_on_tick(&state, &host.frame);
  assert(state.current == owner);
  assert(scheduler_mutex_lock(&state, &mutex) == SCHED_OK);
  assert(scheduler_mutex_lock(&state, &mutex) == SCHED_DEADLOCK);
  assert(scheduler_mutex_unlock(&state, &unused) == SCHED_NOT_OWNER);
  assert(scheduler_create(&state, noop, 0, 3, &waiter) == SCHED_OK);
  (void)scheduler_on_tick(&state, &host.frame);
  assert(state.current == waiter);
  assert(scheduler_mutex_lock(&state, &mutex) == SCHED_BLOCKED);
  assert(mutex.owner == owner);
  assert(owner->effective_priority == 3);
  assert(scheduler_exit(&state) == SCHED_BAD_STATE);
  assert(scheduler_mutex_unlock(&state, &mutex) == SCHED_OK);
  assert(mutex.owner == waiter);
  assert(state.current == waiter);
  assert(owner->effective_priority == owner->base_priority);
}

static void test_deadlock_and_oom(void) {
  host_context host;
  scheduler state;
  scheduler_thread *thread = 0;
  setup(&host, &state);
  host.fail_after = host.allocations + 1;
  assert(scheduler_create(&state, noop, 0, 1, &thread) == SCHED_OUT_OF_MEMORY);
  assert(thread == 0);
  assert(host.allocations == 3); /* bootstrap and idle TCB/stack remain */
  assert(scheduler_begin(&state) == SCHED_OK);
  assert(scheduler_yield(&state) == SCHED_OK);
}

static void test_mutex_cycle_and_transitive_inheritance(void) {
  host_context host;
  scheduler state;
  scheduler_thread *a, *b;
  scheduler_mutex first, second;
  setup(&host, &state);
  scheduler_mutex_init(&first);
  scheduler_mutex_init(&second);
  assert(scheduler_create(&state, noop, 0, 3, &a) == SCHED_OK);
  assert(scheduler_create(&state, noop, 0, 2, &b) == SCHED_OK);
  assert(scheduler_begin(&state) == SCHED_OK);
  (void)scheduler_on_tick(&state, &host.frame);
  assert(state.current == a);
  assert(scheduler_mutex_lock(&state, &first) == SCHED_OK);
  assert(scheduler_set_priority(a, 1) == SCHED_OK);
  (void)scheduler_on_tick(&state, &host.frame);
  assert(state.current == b);
  assert(scheduler_mutex_lock(&state, &second) == SCHED_OK);
  assert(scheduler_mutex_lock(&state, &first) == SCHED_BLOCKED);
  assert(state.current == a);
  assert(a->effective_priority == 2 && first.owner == a);
  assert(scheduler_mutex_lock(&state, &second) == SCHED_DEADLOCK);
  assert(scheduler_mutex_unlock(&state, &first) == SCHED_OK);
  assert(first.owner == b && state.current == b);
  assert(scheduler_mutex_unlock(&state, &second) == SCHED_OK);
  assert(b->effective_priority == b->base_priority);
}

static void test_transitive_priority_inheritance(void) {
  host_context host;
  scheduler state;
  scheduler_thread *a, *b, *c;
  scheduler_mutex first, second;
  setup(&host, &state);
  scheduler_mutex_init(&first);
  scheduler_mutex_init(&second);
  assert(scheduler_create(&state, noop, 0, 3, &a) == SCHED_OK);
  assert(scheduler_begin(&state) == SCHED_OK);
  (void)scheduler_on_tick(&state, &host.frame);
  assert(state.current == a);
  assert(scheduler_mutex_lock(&state, &first) == SCHED_OK);
  assert(scheduler_set_priority(a, 0) == SCHED_OK);
  assert(scheduler_create(&state, noop, 0, 2, &b) == SCHED_OK);
  (void)scheduler_on_tick(&state, &host.frame);
  assert(state.current == b);
  assert(scheduler_mutex_lock(&state, &second) == SCHED_OK);
  assert(scheduler_mutex_lock(&state, &first) == SCHED_BLOCKED);
  assert(state.current == a && a->effective_priority == 2);
  assert(scheduler_create(&state, noop, 0, 3, &c) == SCHED_OK);
  (void)scheduler_on_tick(&state, &host.frame);
  assert(state.current == c);
  assert(scheduler_mutex_lock(&state, &second) == SCHED_BLOCKED);
  assert(state.current == a);
  assert(a->effective_priority == 3 && b->effective_priority == 3);
  assert(scheduler_mutex_unlock(&state, &first) == SCHED_OK);
  assert(state.current == b && first.owner == b);
  assert(scheduler_mutex_unlock(&state, &second) == SCHED_OK);
  assert(state.current == c && second.owner == c);
  assert(b->effective_priority == b->base_priority);
  assert(scheduler_set_priority(c, 0) == SCHED_OK);
  (void)scheduler_on_tick(&state, &host.frame);
  assert(state.current == b);
  assert(scheduler_mutex_unlock(&state, &first) == SCHED_OK);
  assert(a->effective_priority == a->base_priority);
}

static void test_mutex_fifo_for_equal_priority_waiters(void) {
  host_context host;
  scheduler state;
  scheduler_thread *owner, *first, *second;
  scheduler_mutex mutex;
  setup(&host, &state);
  scheduler_mutex_init(&mutex);
  assert(scheduler_create(&state, noop, 0, 3, &owner) == SCHED_OK);
  assert(scheduler_begin(&state) == SCHED_OK);
  (void)scheduler_on_tick(&state, &host.frame);
  assert(state.current == owner);
  assert(scheduler_mutex_lock(&state, &mutex) == SCHED_OK);
  assert(scheduler_set_priority(owner, 0) == SCHED_OK);
  assert(scheduler_create(&state, noop, 0, 2, &first) == SCHED_OK);
  assert(scheduler_create(&state, noop, 0, 2, &second) == SCHED_OK);
  assert(scheduler_yield(&state) == SCHED_OK);
  assert(state.current == first);
  assert(scheduler_mutex_lock(&state, &mutex) == SCHED_BLOCKED);
  assert(state.current == second);
  assert(scheduler_mutex_lock(&state, &mutex) == SCHED_BLOCKED);
  assert(state.current == owner);
  assert(mutex.wait_head == first && first->wait_next == second);
  assert(scheduler_set_priority(first, first->base_priority) == SCHED_OK);
  assert(mutex.wait_head == first && first->wait_next == second);
  assert(scheduler_mutex_unlock(&state, &mutex) == SCHED_OK);
  assert(state.current == first && mutex.owner == first);
  assert(scheduler_mutex_unlock(&state, &mutex) == SCHED_OK);
  assert(mutex.owner == second);
  assert(scheduler_yield(&state) == SCHED_OK);
  assert(state.current == second);
  assert(scheduler_mutex_unlock(&state, &mutex) == SCHED_OK);
}

int main(void) {
  test_priority_rr();
  test_aging();
  test_mutex_handoff_and_donation();
  test_deadlock_and_oom();
  test_mutex_cycle_and_transitive_inheritance();
  test_transitive_priority_inheritance();
  test_mutex_fifo_for_equal_priority_waiters();
  return 0;
}
