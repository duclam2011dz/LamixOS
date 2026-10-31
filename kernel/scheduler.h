/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef LAMIX_KERNEL_SCHEDULER_H
#define LAMIX_KERNEL_SCHEDULER_H

#include <stddef.h>
#include <stdint.h>

#define SCHED_PRIORITY_LEVELS 4u
#define SCHED_PRIORITY_MAX 3u
#define SCHED_STACK_SIZE 16384u
#define SCHED_AGE_TICKS 100u

typedef struct scheduler scheduler;
typedef struct scheduler_thread scheduler_thread;
typedef struct scheduler_mutex scheduler_mutex;
typedef void (*scheduler_entry)(void *argument);

typedef enum scheduler_status {
  SCHED_OK,
  SCHED_INVALID,
  SCHED_OUT_OF_MEMORY,
  SCHED_BAD_STATE,
  SCHED_DEADLOCK,
  SCHED_NOT_OWNER,
  SCHED_BLOCKED
} scheduler_status;

typedef enum scheduler_thread_state {
  SCHED_THREAD_READY,
  SCHED_THREAD_RUNNING,
  SCHED_THREAD_BLOCKED,
  SCHED_THREAD_TERMINATED
} scheduler_thread_state;

typedef struct scheduler_frame {
  uintptr_t r15, r14, r13, r12, r11, r10, r9, r8;
  uintptr_t rdi, rsi, rbp, rbx, rdx, rcx, rax;
  uintptr_t vector, error, rip, cs, rflags, rsp, ss;
} scheduler_frame;

typedef struct scheduler_operations {
  void *context;
  void *(*allocate)(void *context, size_t size);
  void (*deallocate)(void *context, void *pointer);
  uintptr_t (*irq_save)(void *context);
  void (*irq_restore)(void *context, uintptr_t flags);
  /* Returns after the caller is scheduled again; host models may return 0. */
  int (*request_reschedule)(void *context);
  void (*fatal)(void *context, uint64_t reason);
} scheduler_operations;

struct scheduler_thread {
  scheduler *owner;
  scheduler_entry entry;
  void *argument;
  void *stack;
  scheduler_frame *frame;
  scheduler_thread *ready_prev, *ready_next;
  scheduler_thread *retired_next;
  scheduler_thread *wait_prev, *wait_next;
  scheduler_mutex *waiting_on;
  scheduler_mutex *owned_mutexes;
  uint64_t id;
  uint64_t ready_since;
  uint32_t age_ticks;
  uint8_t base_priority;
  uint8_t effective_priority;
  uint8_t age_priority;
  scheduler_thread_state state;
  uint8_t idle;
};

struct scheduler_mutex {
  scheduler_thread *owner;
  scheduler_thread *wait_head, *wait_tail;
  scheduler_mutex *owner_prev, *owner_next;
};

struct scheduler {
  scheduler_operations operations;
  scheduler_thread *ready_head[SCHED_PRIORITY_LEVELS];
  scheduler_thread *ready_tail[SCHED_PRIORITY_LEVELS];
  scheduler_thread *current, *bootstrap, *idle, *retired;
  uint64_t ticks, next_id, live_threads;
  uint8_t active;
};

scheduler_status scheduler_init(scheduler *state,
                                const scheduler_operations *operations,
                                scheduler_entry idle_entry);
scheduler_status scheduler_create(scheduler *state, scheduler_entry entry,
                                  void *argument, uint8_t priority,
                                  scheduler_thread **thread_out);
scheduler_status scheduler_begin(scheduler *state);
scheduler_status scheduler_yield(scheduler *state);
scheduler_status scheduler_exit(scheduler *state);
scheduler_status scheduler_set_priority(scheduler_thread *thread,
                                         uint8_t priority);
void *scheduler_on_tick(scheduler *state, void *interrupt_frame);
void *scheduler_on_reschedule(scheduler *state, void *interrupt_frame);
void scheduler_mutex_init(scheduler_mutex *mutex);
scheduler_status scheduler_mutex_lock(scheduler *state,
                                      scheduler_mutex *mutex);
scheduler_status scheduler_mutex_unlock(scheduler *state,
                                        scheduler_mutex *mutex);
uint64_t scheduler_tick_count(const scheduler *state);

#endif
