/* SPDX-License-Identifier: GPL-3.0-only */
#include "scheduler.h"

#define KERNEL_CS ((uintptr_t)0x08u)
#define INITIAL_RFLAGS ((uintptr_t)0x202u)

_Static_assert(sizeof(scheduler_frame) == 176, "interrupt frame layout");
_Static_assert(offsetof(scheduler_frame, vector) == 120, "vector offset");
_Static_assert(offsetof(scheduler_frame, rip) == 136, "RIP offset");
_Static_assert(SCHED_PRIORITY_MAX + 1u == SCHED_PRIORITY_LEVELS,
               "priority levels");

static void zero_memory(void *pointer, size_t length) {
  uint8_t *bytes = pointer;
  for (size_t i = 0; i < length; ++i) bytes[i] = 0;
}

static uintptr_t lock_scheduler(scheduler *state) {
  return state->operations.irq_save(state->operations.context);
}

static void unlock_scheduler(scheduler *state, uintptr_t flags) {
  state->operations.irq_restore(state->operations.context, flags);
}

static scheduler_thread *new_thread(scheduler *state, scheduler_entry entry,
                                    void *argument, uint8_t priority,
                                    int idle) {
  scheduler_thread *thread = state->operations.allocate(
      state->operations.context, sizeof(*thread));
  if (!thread)
    return 0;
  zero_memory(thread, sizeof(*thread));
  thread->stack = state->operations.allocate(state->operations.context,
                                              SCHED_STACK_SIZE);
  if (!thread->stack) {
    state->operations.deallocate(state->operations.context, thread);
    return 0;
  }
  uintptr_t top = ((uintptr_t)thread->stack + SCHED_STACK_SIZE) &
                  ~(uintptr_t)15;
  thread->kernel_stack_top = top;
  top -= sizeof(uintptr_t); /* SysV function entry requires RSP % 16 == 8. */
  thread->frame = (scheduler_frame *)(top - sizeof(*thread->frame));
  zero_memory(thread->frame, sizeof(*thread->frame));
  thread->frame->rdi = (uintptr_t)thread;
  thread->frame->rip = (uintptr_t)entry;
  thread->frame->cs = KERNEL_CS;
  thread->frame->rflags = INITIAL_RFLAGS;
  thread->frame->rsp = top;
  thread->frame->ss = (uintptr_t)0x10u;
  thread->owner = state;
  thread->entry = entry;
  thread->argument = argument;
  thread->base_priority = priority;
  thread->effective_priority = priority;
  thread->age_priority = priority;
  thread->state = SCHED_THREAD_READY;
  thread->idle = idle != 0;
  thread->id = ++state->next_id;
  ++state->live_threads;
  return thread;
}

static void queue_append(scheduler *state, scheduler_thread *thread) {
  uint8_t priority = thread->effective_priority;
  thread->ready_prev = state->ready_tail[priority];
  thread->ready_next = 0;
  if (thread->ready_prev)
    thread->ready_prev->ready_next = thread;
  else
    state->ready_head[priority] = thread;
  state->ready_tail[priority] = thread;
  thread->state = SCHED_THREAD_READY;
  thread->ready_since = state->ticks;
  thread->age_ticks = 0;
}

static void queue_remove(scheduler *state, scheduler_thread *thread) {
  uint8_t priority = thread->effective_priority;
  if (thread->ready_prev)
    thread->ready_prev->ready_next = thread->ready_next;
  else
    state->ready_head[priority] = thread->ready_next;
  if (thread->ready_next)
    thread->ready_next->ready_prev = thread->ready_prev;
  else
    state->ready_tail[priority] = thread->ready_prev;
  thread->ready_prev = thread->ready_next = 0;
}

static void reprioritize(scheduler_thread *thread);

static scheduler_thread *queue_take_highest(scheduler *state) {
  for (int priority = (int)SCHED_PRIORITY_MAX; priority >= 0; --priority) {
    scheduler_thread *thread = state->ready_head[priority];
    if (thread) {
      queue_remove(state, thread);
      thread->state = SCHED_THREAD_RUNNING;
      thread->age_priority = thread->base_priority;
      thread->age_ticks = 0;
      reprioritize(thread);
      return thread;
    }
  }
  state->idle->state = SCHED_THREAD_RUNNING;
  return state->idle;
}

static scheduler_thread *highest_ready(scheduler *state) {
  for (int priority = (int)SCHED_PRIORITY_MAX; priority >= 0; --priority)
    if (state->ready_head[priority])
      return state->ready_head[priority];
  return 0;
}

static uint8_t thread_priority(scheduler_thread *thread) {
  uint8_t priority = thread->base_priority > thread->age_priority
                         ? thread->base_priority
                         : thread->age_priority;
  for (scheduler_mutex *mutex = thread->owned_mutexes; mutex;
       mutex = mutex->owner_next) {
    if (mutex->wait_head &&
        mutex->wait_head->effective_priority > priority)
      priority = mutex->wait_head->effective_priority;
  }
  return priority;
}

static void reprioritize(scheduler_thread *thread) {
  scheduler *state = thread->owner;
  uint8_t old_priority = thread->effective_priority;
  uint8_t new_priority = thread_priority(thread);
  if (old_priority != new_priority) {
    if (thread->state == SCHED_THREAD_READY) {
      uint64_t ready_since = thread->ready_since;
      uint32_t age_ticks = thread->age_ticks;
      queue_remove(state, thread);
      thread->effective_priority = new_priority;
      queue_append(state, thread);
      thread->ready_since = ready_since;
      thread->age_ticks = age_ticks;
    } else {
      thread->effective_priority = new_priority;
    }
  }
  if (thread->waiting_on && old_priority != new_priority) {
    scheduler_mutex *mutex = thread->waiting_on;
    scheduler_thread *owner = mutex->owner;
    /* Reinsert because effective-priority order may have changed. */
    scheduler_thread *at = mutex->wait_head;
    while (at && at != thread)
      at = at->wait_next;
    if (at) {
      if (thread->wait_prev)
        thread->wait_prev->wait_next = thread->wait_next;
      else
        mutex->wait_head = thread->wait_next;
      if (thread->wait_next)
        thread->wait_next->wait_prev = thread->wait_prev;
      else
        mutex->wait_tail = thread->wait_prev;
      thread->wait_prev = thread->wait_next = 0;
      scheduler_thread *next = mutex->wait_head;
      while (next && next->effective_priority >= thread->effective_priority)
        next = next->wait_next;
      if (!next) {
        thread->wait_prev = mutex->wait_tail;
        if (mutex->wait_tail)
          mutex->wait_tail->wait_next = thread;
        else
          mutex->wait_head = thread;
        mutex->wait_tail = thread;
      } else {
        thread->wait_next = next;
        thread->wait_prev = next->wait_prev;
        if (next->wait_prev)
          next->wait_prev->wait_next = thread;
        else
          mutex->wait_head = thread;
        next->wait_prev = thread;
      }
    }
    if (owner && owner != thread)
      reprioritize(owner);
  }
}

static void age_ready_threads(scheduler *state) {
  for (unsigned p = 0; p < SCHED_PRIORITY_LEVELS; ++p) {
    scheduler_thread *thread = state->ready_head[p];
    while (thread) {
      scheduler_thread *next = thread->ready_next;
      uint64_t waited = state->ticks - thread->ready_since;
      if (waited >= SCHED_AGE_TICKS &&
          thread->age_priority < SCHED_PRIORITY_MAX) {
        uint64_t promotions = waited / SCHED_AGE_TICKS;
        uint64_t room = SCHED_PRIORITY_MAX - thread->age_priority;
        if (promotions > room)
          promotions = room;
        thread->age_priority = (uint8_t)(thread->age_priority + promotions);
        thread->ready_since += promotions * SCHED_AGE_TICKS;
        reprioritize(thread);
      }
      thread = next;
    }
  }
}

static void add_retired(scheduler *state, scheduler_thread *thread) {
  thread->retired_next = state->retired;
  state->retired = thread;
}

static void reap_retired(scheduler *state, scheduler_thread *current) {
  scheduler_thread **link = &state->retired;
  while (*link) {
    scheduler_thread *thread = *link;
    if (thread == current) {
      link = &thread->retired_next;
      continue;
    }
    *link = thread->retired_next;
    if (thread->stack)
      state->operations.deallocate(state->operations.context, thread->stack);
    state->operations.deallocate(state->operations.context, thread);
    --state->live_threads;
  }
}

void scheduler_collect_retired(scheduler *state) {
  if (!state || !state->active || !state->current)
    return;
  uintptr_t flags = lock_scheduler(state);
  reap_retired(state, state->current);
  unlock_scheduler(state, flags);
}

static void *dispatch(scheduler *state, void *frame, int tick, int terminate) {
  if (!state || !state->active || !state->current)
    return frame;
  scheduler_thread *current = state->current;
  current->frame = (scheduler_frame *)frame;
  reap_retired(state, current);
  if (terminate && current != state->bootstrap && !current->idle) {
    current->state = SCHED_THREAD_TERMINATED;
    add_retired(state, current);
  }
  if (tick) {
    ++state->ticks;
    age_ready_threads(state);
  }
  if (current->state == SCHED_THREAD_RUNNING && !current->idle)
    queue_append(state, current);
  scheduler_thread *next = queue_take_highest(state);
  if (!next)
    return frame;
  state->current = next;
  if (state->operations.context_switch)
    state->operations.context_switch(state->operations.context, next);
  return next->frame ? next->frame : frame;
}

static void thread_trampoline(void *argument) {
  scheduler_thread *thread = (scheduler_thread *)argument;
  scheduler *state = thread->owner;
  thread->entry(thread->argument);
  scheduler_status status = scheduler_exit(state);
  if (status != SCHED_OK && state->operations.fatal)
    state->operations.fatal(state->operations.context, (uint64_t)status);
  for (;;)
    __asm__ volatile("cli; hlt" ::: "memory");
}

scheduler_status scheduler_init(scheduler *state,
                                const scheduler_operations *operations,
                                scheduler_entry idle_entry) {
  if (!state || !operations || !operations->allocate ||
      !operations->deallocate || !operations->irq_save ||
      !operations->irq_restore || !operations->request_reschedule ||
      !idle_entry)
    return SCHED_INVALID;
  zero_memory(state, sizeof(*state));
  state->operations = *operations;
  scheduler_thread *bootstrap = state->operations.allocate(
      state->operations.context, sizeof(*bootstrap));
  if (!bootstrap)
    return SCHED_OUT_OF_MEMORY;
  zero_memory(bootstrap, sizeof(*bootstrap));
  bootstrap->owner = state;
  bootstrap->id = ++state->next_id;
  bootstrap->state = SCHED_THREAD_RUNNING;
  bootstrap->base_priority = 0;
  bootstrap->effective_priority = 0;
  bootstrap->age_priority = 0;
  ++state->live_threads;
  scheduler_thread *idle = new_thread(state, idle_entry, 0, 0, 1);
  if (!idle) {
    state->operations.deallocate(state->operations.context, bootstrap);
    state->live_threads = 0;
    return SCHED_OUT_OF_MEMORY;
  }
  state->bootstrap = state->current = bootstrap;
  state->idle = idle;
  return SCHED_OK;
}

scheduler_status scheduler_create(scheduler *state, scheduler_entry entry,
                                  void *argument, uint8_t priority,
                                  scheduler_thread **thread_out) {
  if (!state || !entry || priority > SCHED_PRIORITY_MAX || !thread_out)
    return SCHED_INVALID;
  *thread_out = 0;
  uintptr_t flags = lock_scheduler(state);
  scheduler_thread *thread = new_thread(state, thread_trampoline, 0,
                                        priority, 0);
  if (!thread) {
    unlock_scheduler(state, flags);
    return SCHED_OUT_OF_MEMORY;
  }
  thread->entry = entry;
  thread->argument = argument;
  thread->frame->rdi = (uintptr_t)thread;
  thread->frame->rip = (uintptr_t)thread_trampoline;
  queue_append(state, thread);
  *thread_out = thread;
  unlock_scheduler(state, flags);
  return SCHED_OK;
}

scheduler_status scheduler_begin(scheduler *state) {
  if (!state || !state->current || state->active)
    return SCHED_BAD_STATE;
  uintptr_t flags = lock_scheduler(state);
  state->active = 1;
  unlock_scheduler(state, flags);
  return SCHED_OK;
}

void *scheduler_on_tick(scheduler *state, void *interrupt_frame) {
  return dispatch(state, interrupt_frame, 1, 0);
}

void *scheduler_on_reschedule(scheduler *state, void *interrupt_frame) {
  return dispatch(state, interrupt_frame, 0, 0);
}

void *scheduler_terminate_interrupt(scheduler *state, void *interrupt_frame) {
  return dispatch(state, interrupt_frame, 0, 1);
}

scheduler_status scheduler_configure_user(scheduler_thread *thread,
                                          uintptr_t entry,
                                          uintptr_t user_stack) {
  if (!thread || !thread->owner || !entry || !user_stack ||
      thread->state != SCHED_THREAD_READY || !thread->frame)
    return SCHED_INVALID;
  thread->frame->rip = entry;
  thread->frame->cs = (uintptr_t)0x23u;
  thread->frame->rflags = INITIAL_RFLAGS & ~(uintptr_t)(3u << 12);
  thread->frame->rsp = user_stack;
  thread->frame->ss = (uintptr_t)0x1bu;
  thread->user_mode = 1;
  return SCHED_OK;
}

scheduler_status scheduler_yield(scheduler *state) {
  if (!state || !state->active || !state->current)
    return SCHED_BAD_STATE;
  uintptr_t flags = lock_scheduler(state);
  state->operations.request_reschedule(state->operations.context);
  unlock_scheduler(state, flags);
  return SCHED_OK;
}

scheduler_status scheduler_exit(scheduler *state) {
  if (!state || !state->active || !state->current ||
      state->current == state->bootstrap || state->current->idle)
    return SCHED_BAD_STATE;
  uintptr_t flags = lock_scheduler(state);
  scheduler_thread *thread = state->current;
  if (thread->owned_mutexes) {
    unlock_scheduler(state, flags);
    return SCHED_BAD_STATE;
  }
  thread->state = SCHED_THREAD_TERMINATED;
  add_retired(state, thread);
  state->operations.request_reschedule(state->operations.context);
  unlock_scheduler(state, flags);
  return SCHED_OK;
}

scheduler_status scheduler_set_priority(scheduler_thread *thread,
                                         uint8_t priority) {
  if (!thread || priority > SCHED_PRIORITY_MAX || !thread->owner)
    return SCHED_INVALID;
  scheduler *state = thread->owner;
  uintptr_t flags = lock_scheduler(state);
  if (thread->state == SCHED_THREAD_TERMINATED) {
    unlock_scheduler(state, flags);
    return SCHED_BAD_STATE;
  }
  thread->base_priority = priority;
  thread->age_priority = priority;
  reprioritize(thread);
  unlock_scheduler(state, flags);
  return SCHED_OK;
}

void scheduler_mutex_init(scheduler_mutex *mutex) {
  if (mutex)
    zero_memory(mutex, sizeof(*mutex));
}

static int mutex_cycle(scheduler_thread *current, scheduler_thread *owner) {
  uint64_t limit = current->owner->live_threads + 1;
  while (owner && limit--) {
    if (owner == current)
      return 1;
    scheduler_mutex *waiting = owner->waiting_on;
    owner = waiting ? waiting->owner : 0;
  }
  return owner != 0;
}

static void mutex_wait_insert(scheduler_mutex *mutex,
                              scheduler_thread *thread) {
  scheduler_thread *at = mutex->wait_head;
  while (at && at->effective_priority >= thread->effective_priority)
    at = at->wait_next;
  if (!at) {
    thread->wait_prev = mutex->wait_tail;
    if (mutex->wait_tail)
      mutex->wait_tail->wait_next = thread;
    else
      mutex->wait_head = thread;
    mutex->wait_tail = thread;
  } else {
    thread->wait_next = at;
    thread->wait_prev = at->wait_prev;
    if (at->wait_prev)
      at->wait_prev->wait_next = thread;
    else
      mutex->wait_head = thread;
    at->wait_prev = thread;
  }
  thread->waiting_on = mutex;
}

static void owned_add(scheduler_thread *thread, scheduler_mutex *mutex) {
  mutex->owner = thread;
  mutex->owner_prev = 0;
  mutex->owner_next = thread->owned_mutexes;
  if (mutex->owner_next)
    mutex->owner_next->owner_prev = mutex;
  thread->owned_mutexes = mutex;
}

static void owned_remove(scheduler_thread *thread, scheduler_mutex *mutex) {
  if (mutex->owner_prev)
    mutex->owner_prev->owner_next = mutex->owner_next;
  else
    thread->owned_mutexes = mutex->owner_next;
  if (mutex->owner_next)
    mutex->owner_next->owner_prev = mutex->owner_prev;
  mutex->owner_prev = mutex->owner_next = 0;
  mutex->owner = 0;
}

scheduler_status scheduler_mutex_lock(scheduler *state,
                                      scheduler_mutex *mutex) {
  if (!state || !mutex || !state->active)
    return SCHED_INVALID;
  uintptr_t flags = lock_scheduler(state);
  scheduler_thread *current = state->current;
  if (!current || current->idle || current->state != SCHED_THREAD_RUNNING) {
    unlock_scheduler(state, flags);
    return SCHED_BAD_STATE;
  }
  if (!mutex->owner) {
    owned_add(current, mutex);
    unlock_scheduler(state, flags);
    return SCHED_OK;
  }
  if (mutex->owner == current || mutex_cycle(current, mutex->owner)) {
    unlock_scheduler(state, flags);
    return SCHED_DEADLOCK;
  }
  mutex_wait_insert(mutex, current);
  current->state = SCHED_THREAD_BLOCKED;
  reprioritize(mutex->owner);
  int resumed = state->operations.request_reschedule(state->operations.context);
  unlock_scheduler(state, flags);
  return resumed ? SCHED_OK : SCHED_BLOCKED;
}

scheduler_status scheduler_mutex_unlock(scheduler *state,
                                        scheduler_mutex *mutex) {
  if (!state || !mutex)
    return SCHED_INVALID;
  uintptr_t flags = lock_scheduler(state);
  scheduler_thread *current = state->current;
  if (!current || mutex->owner != current) {
    unlock_scheduler(state, flags);
    return SCHED_NOT_OWNER;
  }
  owned_remove(current, mutex);
  scheduler_thread *waiter = mutex->wait_head;
  if (waiter) {
    mutex->wait_head = waiter->wait_next;
    if (mutex->wait_head)
      mutex->wait_head->wait_prev = 0;
    else
      mutex->wait_tail = 0;
    waiter->wait_prev = waiter->wait_next = 0;
    waiter->waiting_on = 0;
    owned_add(waiter, mutex);
    queue_append(state, waiter);
  }
  reprioritize(current);
  scheduler_thread *next = highest_ready(state);
  if (next && next->effective_priority > current->effective_priority)
    state->operations.request_reschedule(state->operations.context);
  unlock_scheduler(state, flags);
  return SCHED_OK;
}

uint64_t scheduler_tick_count(const scheduler *state) {
  return state ? state->ticks : 0;
}
