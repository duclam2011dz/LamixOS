/* SPDX-License-Identifier: GPL-3.0-only */
#include "scheduler.h"

#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>

#define THREADS 48u
#define TICKS 30000u

static _Thread_local uint32_t host_cpu;
static _Atomic uint64_t switches;

static void *allocate(void *context, size_t size) {
  (void)context;
  return malloc(size);
}
static void deallocate(void *context, void *pointer) {
  (void)context;
  free(pointer);
}
static uintptr_t irq_save(void *context) {
  (void)context;
  return 0;
}
static void irq_restore(void *context, uintptr_t flags) {
  (void)context;
  (void)flags;
}
static int reschedule(void *context) {
  (void)context;
  return 1;
}
static void fatal(void *context, uint64_t reason) {
  (void)context;
  (void)reason;
  abort();
}
static void context_switch(void *context, scheduler_thread *thread) {
  (void)context;
  (void)thread;
  atomic_fetch_add_explicit(&switches, 1, memory_order_relaxed);
}
static uint32_t cpu_id(void *context) {
  (void)context;
  return host_cpu;
}
static void idle(void *argument) { (void)argument; }
static void worker(void *argument) { (void)argument; }

typedef struct dispatch_context {
  scheduler *state;
  uint32_t cpu;
} dispatch_context;

static void *dispatch_loop(void *argument) {
  dispatch_context *context = argument;
  host_cpu = context->cpu;
  scheduler_frame frame = {0};
  for (uint32_t i = 0; i < TICKS; ++i)
    (void)scheduler_on_tick(context->state, &frame);
  return 0;
}

static void *affinity_loop(void *argument) {
  scheduler_thread **threads = argument;
  host_cpu = 0;
  for (uint32_t i = 0; i < TICKS; ++i) {
    uint64_t mask = (i & 1u) ? UINT64_C(3) : UINT64_C(1);
    scheduler_status status = scheduler_set_affinity(
        threads[i % THREADS], mask);
    assert(status == SCHED_OK);
  }
  for (uint32_t i = 0; i < THREADS; ++i)
    assert(scheduler_set_affinity(threads[i], UINT64_C(3)) == SCHED_OK);
  return 0;
}

int main(void) {
  scheduler state;
  scheduler_operations operations = {
      .context = 0,
      .allocate = allocate,
      .deallocate = deallocate,
      .irq_save = irq_save,
      .irq_restore = irq_restore,
      .request_reschedule = reschedule,
      .fatal = fatal,
      .context_switch = context_switch,
      .cpu_id = cpu_id,
      .irq_wait = 0,
  };
  host_cpu = 0;
  assert(scheduler_init_smp(&state, &operations, idle, 2) == SCHED_OK);
  assert(scheduler_begin(&state) == SCHED_OK);
  assert(scheduler_cpu_online(&state, 1, UINT64_C(0xffff800000010000)) ==
         SCHED_OK);
  scheduler_thread *threads[THREADS];
  for (uint32_t i = 0; i < THREADS; ++i) {
    assert(scheduler_create(&state, worker, 0, 1, &threads[i]) == SCHED_OK);
    assert(scheduler_set_affinity(threads[i], UINT64_C(1)) == SCHED_OK);
  }
  for (uint32_t i = 0; i < THREADS; ++i)
    threads[i]->user_mode = 1;
  for (uint32_t i = 0; i < THREADS; ++i)
    assert(scheduler_set_affinity(threads[i], UINT64_C(3)) == SCHED_OK);

  scheduler_frame first = {0}, second = {0};
  host_cpu = 0;
  (void)scheduler_on_tick(&state, &first);
  host_cpu = 1;
  (void)scheduler_on_tick(&state, &second);
  assert(scheduler_work_steals(&state) > 0);

  dispatch_context contexts[2] = {{&state, 0}, {&state, 1}};
  pthread_t cpu_threads[2], affinity_thread;
  assert(pthread_create(&cpu_threads[0], 0, dispatch_loop, &contexts[0]) == 0);
  assert(pthread_create(&cpu_threads[1], 0, dispatch_loop, &contexts[1]) == 0);
  assert(pthread_create(&affinity_thread, 0, affinity_loop, threads) == 0);
  assert(pthread_join(cpu_threads[0], 0) == 0);
  assert(pthread_join(cpu_threads[1], 0) == 0);
  assert(pthread_join(affinity_thread, 0) == 0);
  assert(scheduler_observed_cpu_mask(&state) == UINT64_C(3));
  assert(scheduler_work_steals(&state) > 0);
  assert(atomic_load_explicit(&switches, memory_order_relaxed) > 0);
  for (uint32_t i = 0; i < THREADS; ++i) {
    assert(threads[i]->affinity_mask == UINT64_C(3));
    assert(threads[i]->run_cpu < 2);
  }
  return 0;
}
