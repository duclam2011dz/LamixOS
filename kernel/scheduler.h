/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef LAMIX_KERNEL_SCHEDULER_H
#define LAMIX_KERNEL_SCHEDULER_H

#include <stddef.h>
#include <stdint.h>

#define SCHED_PRIORITY_LEVELS 4u
#define SCHED_PRIORITY_MAX 3u
#define SCHED_STACK_SIZE 16384u
#define SCHED_AGE_TICKS 100u
#define SCHED_MAX_CPUS 16u

typedef struct scheduler scheduler;
typedef struct scheduler_thread scheduler_thread;
typedef struct scheduler_mutex scheduler_mutex;
typedef struct scheduler_cpu scheduler_cpu;
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
  void (*context_switch)(void *context, scheduler_thread *next);
  uint32_t (*cpu_id)(void *context);
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
  uint8_t user_mode;
  uintptr_t kernel_stack_top;
  uint64_t affinity_mask;
  uint32_t run_cpu;
  void *private_context;
};

struct scheduler_mutex {
  scheduler_thread *owner;
  scheduler_thread *wait_head, *wait_tail;
  scheduler_mutex *owner_prev, *owner_next;
};

struct scheduler_cpu {
  scheduler_thread *ready_head[SCHED_PRIORITY_LEVELS];
  scheduler_thread *ready_tail[SCHED_PRIORITY_LEVELS];
  scheduler_thread *current, *bootstrap, *idle, *retired;
  uint64_t ticks;
  uint8_t online;
};

struct scheduler {
  scheduler_operations operations;
  union {
    scheduler_cpu cpu0;
    struct {
      scheduler_thread *ready_head[SCHED_PRIORITY_LEVELS];
      scheduler_thread *ready_tail[SCHED_PRIORITY_LEVELS];
      scheduler_thread *current, *bootstrap, *idle, *retired;
      uint64_t ticks;
    };
  };
  scheduler_cpu cpus[SCHED_MAX_CPUS - 1u];
  uint64_t next_id, live_threads;
  uint32_t cpu_limit;
  volatile uint32_t spinlock;
  uint64_t observed_cpu_mask;
  uint64_t work_steals;
  scheduler_entry idle_entry;
  uint8_t active;
};

scheduler_status scheduler_init(scheduler *state,
                                const scheduler_operations *operations,
                                scheduler_entry idle_entry);
scheduler_status scheduler_init_smp(scheduler *state,
                                    const scheduler_operations *operations,
                                    scheduler_entry idle_entry,
                                    uint32_t cpu_limit);
scheduler_status scheduler_cpu_online(scheduler *state, uint32_t cpu_id,
                                      uintptr_t bootstrap_stack_top);
scheduler_thread *scheduler_current(scheduler *state);
scheduler_status scheduler_set_affinity(scheduler_thread *thread,
                                        uint64_t affinity_mask);
void scheduler_critical_enter(scheduler *state, uintptr_t *flags);
void scheduler_critical_leave(scheduler *state, uintptr_t flags);
scheduler_status scheduler_create(scheduler *state, scheduler_entry entry,
                                  void *argument, uint8_t priority,
                                  scheduler_thread **thread_out);
scheduler_status scheduler_configure_user(scheduler_thread *thread,
                                          uintptr_t entry,
                                          uintptr_t user_stack);
void *scheduler_terminate_interrupt(scheduler *state, void *interrupt_frame);
void scheduler_collect_retired(scheduler *state);
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
uint64_t scheduler_observed_cpu_mask(const scheduler *state);
uint64_t scheduler_work_steals(const scheduler *state);

#endif
