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
  uintptr_t flags = state->operations.irq_save(state->operations.context);
  while (__atomic_exchange_n(&state->spinlock, 1u, __ATOMIC_ACQUIRE)) {
    if (state->operations.irq_wait)
      state->operations.irq_wait(state->operations.context);
    else
      __asm__ volatile("pause" ::: "memory");
  }
  return flags;
}

static void unlock_scheduler(scheduler *state, uintptr_t flags) {
  __atomic_store_n(&state->spinlock, 0u, __ATOMIC_RELEASE);
  state->operations.irq_restore(state->operations.context, flags);
}

static scheduler_cpu *cpu_state(scheduler *state, uint32_t cpu_id) {
  if (!state || cpu_id >= state->cpu_limit || cpu_id >= SCHED_MAX_CPUS)
    return 0;
  return cpu_id == 0 ? &state->cpu0 : &state->cpus[cpu_id - 1u];
}

static uint32_t current_cpu_id(scheduler *state) {
  uint32_t id = state->operations.cpu_id
                    ? state->operations.cpu_id(state->operations.context) : 0;
  return id < state->cpu_limit ? id : UINT32_MAX;
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

static void queue_append(scheduler *state, scheduler_cpu *cpu,
                         uint32_t cpu_id, scheduler_thread *thread) {
  (void)state;
  uint8_t priority = thread->effective_priority;
  thread->ready_prev = cpu->ready_tail[priority];
  thread->ready_next = 0;
  if (thread->ready_prev)
    thread->ready_prev->ready_next = thread;
  else
    cpu->ready_head[priority] = thread;
  cpu->ready_tail[priority] = thread;
  thread->state = SCHED_THREAD_READY;
  thread->run_cpu = cpu_id;
  thread->ready_since = cpu->ticks;
  thread->age_ticks = 0;
}

static void queue_remove(scheduler *state, scheduler_thread *thread) {
  scheduler_cpu *cpu = cpu_state(state, thread->run_cpu);
  if (!cpu) return;
  uint8_t priority = thread->effective_priority;
  if (thread->ready_prev)
    thread->ready_prev->ready_next = thread->ready_next;
  else
    cpu->ready_head[priority] = thread->ready_next;
  if (thread->ready_next)
    thread->ready_next->ready_prev = thread->ready_prev;
  else
    cpu->ready_tail[priority] = thread->ready_prev;
  thread->ready_prev = thread->ready_next = 0;
}

static void reprioritize(scheduler_thread *thread);

static scheduler_thread *queue_take_highest(scheduler *state,
                                            scheduler_cpu *cpu,
                                            uint32_t cpu_id) {
  for (int priority = (int)SCHED_PRIORITY_MAX; priority >= 0; --priority) {
    scheduler_thread *thread = cpu->ready_head[priority];
    if (thread) {
      queue_remove(state, thread);
      thread->state = SCHED_THREAD_RUNNING;
      thread->run_cpu = cpu_id;
      thread->age_priority = thread->base_priority;
      thread->age_ticks = 0;
      reprioritize(thread);
      return thread;
    }
  }
  for (uint32_t victim_id = 0; victim_id < state->cpu_limit; ++victim_id) {
    if (victim_id == cpu_id) continue;
    scheduler_cpu *victim = cpu_state(state, victim_id);
    if (!victim || !victim->online) continue;
    for (int priority = (int)SCHED_PRIORITY_MAX; priority >= 0; --priority) {
      scheduler_thread *thread = victim->ready_tail[priority];
      while (thread &&
             (thread == victim->switching_from ||
              !(thread->affinity_mask & (UINT64_C(1) << cpu_id))))
        thread = thread->ready_prev;
      if (!thread) continue;
      queue_remove(state, thread);
      thread->state = SCHED_THREAD_RUNNING;
      thread->run_cpu = cpu_id;
      __atomic_add_fetch(&state->work_steals, UINT64_C(1), __ATOMIC_RELAXED);
      thread->age_priority = thread->base_priority;
      thread->age_ticks = 0;
      reprioritize(thread);
      return thread;
    }
  }
  cpu->idle->state = SCHED_THREAD_RUNNING;
  return cpu->idle;
}

static scheduler_thread *highest_ready(scheduler *state, scheduler_cpu *cpu) {
  (void)state;
  for (int priority = (int)SCHED_PRIORITY_MAX; priority >= 0; --priority)
    if (cpu->ready_head[priority])
      return cpu->ready_head[priority];
  return 0;
}

static scheduler_cpu *least_loaded_cpu(scheduler *state, uint64_t mask,
                                       uint32_t *selected) {
  scheduler_cpu *best = 0;
  size_t best_count = SIZE_MAX;
  for (uint32_t id = 0; id < state->cpu_limit; ++id) {
    if (!(mask & (UINT64_C(1) << id))) continue;
    scheduler_cpu *cpu = cpu_state(state, id);
    if (!cpu || !cpu->online) continue;
    size_t count = 0;
    for (unsigned p = 0; p < SCHED_PRIORITY_LEVELS; ++p)
      for (scheduler_thread *t = cpu->ready_head[p]; t; t = t->ready_next)
        ++count;
    if (cpu->current && !cpu->current->idle) ++count;
    if (!best || count < best_count) {
      best = cpu;
      best_count = count;
      *selected = id;
    }
  }
  return best;
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
  scheduler_cpu *cpu = cpu_state(state, thread->run_cpu);
  uint8_t old_priority = thread->effective_priority;
  uint8_t new_priority = thread_priority(thread);
  if (old_priority != new_priority) {
    if (thread->state == SCHED_THREAD_READY && cpu) {
      uint64_t ready_since = thread->ready_since;
      uint32_t age_ticks = thread->age_ticks;
      queue_remove(state, thread);
      thread->effective_priority = new_priority;
      queue_append(state, cpu, thread->run_cpu, thread);
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

static void age_ready_threads(scheduler *state, scheduler_cpu *cpu) {
  (void)state;
  for (unsigned p = 0; p < SCHED_PRIORITY_LEVELS; ++p) {
    scheduler_thread *thread = cpu->ready_head[p];
    while (thread) {
      scheduler_thread *next = thread->ready_next;
      uint64_t waited = cpu->ticks - thread->ready_since;
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

static int is_running_on_any_cpu(scheduler *state, scheduler_thread *thread) {
  for (uint32_t i = 0; i < state->cpu_limit; ++i) {
    scheduler_cpu *cpu = cpu_state(state, i);
    if (cpu && cpu->online &&
        (cpu->current == thread || cpu->switching_from == thread)) return 1;
  }
  return 0;
}

static void reap_retired(scheduler *state) {
  scheduler_thread **link = &state->retired;
  while (*link) {
    scheduler_thread *thread = *link;
    if (is_running_on_any_cpu(state, thread)) {
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
  if (!state || !state->active)
    return;
  uintptr_t flags = lock_scheduler(state);
  reap_retired(state);
  unlock_scheduler(state, flags);
}

static void *dispatch(scheduler *state, void *frame, int tick, int terminate) {
  if (!state || !state->active)
    return frame;
  uint32_t cpu_id = current_cpu_id(state);
  scheduler_cpu *cpu = cpu_state(state, cpu_id);
  if (!cpu || !cpu->online || !cpu->current) return frame;
  uintptr_t flags = lock_scheduler(state);
  scheduler_thread *current = cpu->current;
  current->frame = (scheduler_frame *)frame;
  /* This interrupt proves the previous return path left the old stack. */
  cpu->switching_from = 0;
  reap_retired(state);
  if (terminate && current != cpu->bootstrap && !current->idle) {
    current->state = SCHED_THREAD_TERMINATED;
    add_retired(state, current);
  }
  if (tick) {
    ++cpu->ticks;
    if (cpu_id == 0) state->ticks = cpu->ticks;
    age_ready_threads(state, cpu);
  }
  if (current->state == SCHED_THREAD_RUNNING && !current->idle &&
      !(cpu_id != 0 && current == cpu->bootstrap)) {
    if (current->affinity_mask & (UINT64_C(1) << cpu_id)) {
      queue_append(state, cpu, cpu_id, current);
    } else {
      uint32_t target_id = 0;
      scheduler_cpu *target = least_loaded_cpu(state, current->affinity_mask,
                                               &target_id);
      if (target) queue_append(state, target, target_id, current);
      else current->state = SCHED_THREAD_TERMINATED;
    }
  }
  scheduler_thread *next = queue_take_highest(state, cpu, cpu_id);
  if (!next) {
    unlock_scheduler(state, flags);
    return frame;
  }
  cpu->current = next;
  if (next != current) cpu->switching_from = current;
  if (next->user_mode)
    __atomic_fetch_or(&state->observed_cpu_mask, UINT64_C(1) << cpu_id,
                      __ATOMIC_RELAXED);
  if (cpu_id == 0) state->current = next;
  if (state->operations.context_switch)
    state->operations.context_switch(state->operations.context, next);
  unlock_scheduler(state, flags);
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
  state->cpu_limit = 1;
  state->idle_entry = idle_entry;
  state->cpu0.online = 1;
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
  bootstrap->affinity_mask = 1;
  idle->affinity_mask = 1;
  idle->run_cpu = 0;
  return SCHED_OK;
}

scheduler_status scheduler_init_smp(scheduler *state,
                                    const scheduler_operations *operations,
                                    scheduler_entry idle_entry,
                                    uint32_t cpu_limit) {
  if (!cpu_limit || cpu_limit > SCHED_MAX_CPUS) return SCHED_INVALID;
  scheduler_status status = scheduler_init(state, operations, idle_entry);
  if (status != SCHED_OK) return status;
  state->cpu_limit = cpu_limit;
  state->cpu0.online = 1;
  state->cpu0.current = state->current;
  state->cpu0.bootstrap = state->bootstrap;
  state->cpu0.idle = state->idle;
  return SCHED_OK;
}

scheduler_status scheduler_cpu_online(scheduler *state, uint32_t cpu_id,
                                      uintptr_t bootstrap_stack_top) {
  if (!state || !state->active || !cpu_id || cpu_id >= state->cpu_limit ||
      !bootstrap_stack_top)
    return SCHED_INVALID;
  uintptr_t flags = lock_scheduler(state);
  scheduler_cpu *cpu = cpu_state(state, cpu_id);
  if (!cpu || cpu->online) {
    unlock_scheduler(state, flags);
    return SCHED_BAD_STATE;
  }
  scheduler_thread *bootstrap = state->operations.allocate(
      state->operations.context, sizeof(*bootstrap));
  if (!bootstrap) {
    unlock_scheduler(state, flags);
    return SCHED_OUT_OF_MEMORY;
  }
  zero_memory(bootstrap, sizeof(*bootstrap));
  bootstrap->owner = state;
  bootstrap->id = ++state->next_id;
  bootstrap->state = SCHED_THREAD_RUNNING;
  bootstrap->kernel_stack_top = bootstrap_stack_top;
  bootstrap->run_cpu = cpu_id;
  bootstrap->affinity_mask = UINT64_C(1) << cpu_id;
  ++state->live_threads;
  scheduler_thread *idle = new_thread(state, state->idle_entry, 0, 0, 1);
  if (!idle) {
    state->operations.deallocate(state->operations.context, bootstrap);
    --state->live_threads;
    unlock_scheduler(state, flags);
    return SCHED_OUT_OF_MEMORY;
  }
  idle->run_cpu = cpu_id;
  idle->affinity_mask = UINT64_C(1) << cpu_id;
  cpu->idle = idle;
  cpu->bootstrap = bootstrap;
  cpu->current = bootstrap;
  cpu->online = 1;
  unlock_scheduler(state, flags);
  return SCHED_OK;
}

scheduler_thread *scheduler_current(scheduler *state) {
  if (!state) return 0;
  scheduler_cpu *cpu = cpu_state(state, current_cpu_id(state));
  return cpu && cpu->online ? cpu->current : 0;
}

scheduler_status scheduler_set_affinity(scheduler_thread *thread,
                                        uint64_t affinity_mask) {
  if (!thread || !thread->owner || !affinity_mask) return SCHED_INVALID;
  scheduler *state = thread->owner;
  uint64_t valid_mask = (UINT64_C(1) << state->cpu_limit) - 1u;
  if (affinity_mask & ~valid_mask) return SCHED_INVALID;
  uintptr_t flags = lock_scheduler(state);
  uint64_t online_mask = 0;
  for (uint32_t id = 0; id < state->cpu_limit; ++id) {
    scheduler_cpu *cpu = cpu_state(state, id);
    if (cpu && cpu->online) online_mask |= UINT64_C(1) << id;
  }
  if (!(affinity_mask & online_mask) || (affinity_mask & ~online_mask)) {
    unlock_scheduler(state, flags);
    return SCHED_INVALID;
  }
  if (thread->state == SCHED_THREAD_TERMINATED) {
    unlock_scheduler(state, flags);
    return SCHED_BAD_STATE;
  }
  thread->affinity_mask = affinity_mask;
  int reschedule = 0;
  if (thread->state == SCHED_THREAD_READY &&
      !(affinity_mask & (UINT64_C(1) << thread->run_cpu))) {
    queue_remove(state, thread);
    uint32_t target_id = 0;
    scheduler_cpu *target = least_loaded_cpu(state, affinity_mask, &target_id);
    if (!target) {
      unlock_scheduler(state, flags);
      return SCHED_BAD_STATE;
    }
    queue_append(state, target, target_id, thread);
  } else if (thread->state == SCHED_THREAD_RUNNING) {
    uint32_t cpu_id = current_cpu_id(state);
    if (!(affinity_mask & (UINT64_C(1) << thread->run_cpu)) &&
        cpu_id == thread->run_cpu)
      reschedule = 1;
  }
  unlock_scheduler(state, flags);
  if (reschedule)
    state->operations.request_reschedule(state->operations.context);
  return SCHED_OK;
}

void scheduler_critical_enter(scheduler *state, uintptr_t *flags) {
  if (state && flags) *flags = lock_scheduler(state);
}

void scheduler_critical_leave(scheduler *state, uintptr_t flags) {
  if (state) unlock_scheduler(state, flags);
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
  thread->affinity_mask = (UINT64_C(1) << state->cpu_limit) - 1u;
  uint32_t target_id = 0;
  scheduler_cpu *target = least_loaded_cpu(state, thread->affinity_mask,
                                           &target_id);
  if (!target) {
    state->operations.deallocate(state->operations.context, thread->stack);
    state->operations.deallocate(state->operations.context, thread);
    --state->live_threads;
    unlock_scheduler(state, flags);
    return SCHED_BAD_STATE;
  }
  queue_append(state, target, target_id, thread);
  *thread_out = thread;
  unlock_scheduler(state, flags);
  return SCHED_OK;
}

scheduler_status scheduler_begin(scheduler *state) {
  if (!state || !state->cpu0.current || state->active)
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
  if (!state || !state->active)
    return SCHED_BAD_STATE;
  uintptr_t flags = lock_scheduler(state);
  unlock_scheduler(state, flags);
  state->operations.request_reschedule(state->operations.context);
  return SCHED_OK;
}

scheduler_status scheduler_exit(scheduler *state) {
  if (!state || !state->active)
    return SCHED_BAD_STATE;
  uintptr_t flags = lock_scheduler(state);
  uint32_t cpu_id = current_cpu_id(state);
  scheduler_cpu *cpu = cpu_state(state, cpu_id);
  scheduler_thread *thread = cpu ? cpu->current : 0;
  if (!thread || thread == cpu->bootstrap || thread->idle) {
    unlock_scheduler(state, flags);
    return SCHED_BAD_STATE;
  }
  if (thread->owned_mutexes) {
    unlock_scheduler(state, flags);
    return SCHED_BAD_STATE;
  }
  thread->state = SCHED_THREAD_TERMINATED;
  add_retired(state, thread);
  unlock_scheduler(state, flags);
  state->operations.request_reschedule(state->operations.context);
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
  uint32_t cpu_id = current_cpu_id(state);
  scheduler_cpu *cpu = cpu_state(state, cpu_id);
  scheduler_thread *current = cpu ? cpu->current : 0;
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
  unlock_scheduler(state, flags);
  int resumed = state->operations.request_reschedule(state->operations.context);
  return resumed ? SCHED_OK : SCHED_BLOCKED;
}

scheduler_status scheduler_mutex_unlock(scheduler *state,
                                        scheduler_mutex *mutex) {
  if (!state || !mutex)
    return SCHED_INVALID;
  uintptr_t flags = lock_scheduler(state);
  uint32_t cpu_id = current_cpu_id(state);
  scheduler_cpu *cpu = cpu_state(state, cpu_id);
  scheduler_thread *current = cpu ? cpu->current : 0;
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
    uint32_t target_id = 0;
    scheduler_cpu *target = least_loaded_cpu(state, waiter->affinity_mask,
                                             &target_id);
    if (!target) {
      unlock_scheduler(state, flags);
      return SCHED_BAD_STATE;
    }
    queue_append(state, target, target_id, waiter);
  }
  reprioritize(current);
  scheduler_thread *next = highest_ready(state, cpu);
  int reschedule = next && next->effective_priority > current->effective_priority;
  unlock_scheduler(state, flags);
  if (reschedule)
    state->operations.request_reschedule(state->operations.context);
  return SCHED_OK;
}

uint64_t scheduler_tick_count(const scheduler *state) {
  if (!state) return 0;
  scheduler *mutable_state = (scheduler *)state;
  scheduler_cpu *cpu = cpu_state(mutable_state, current_cpu_id(mutable_state));
  return cpu ? cpu->ticks : 0;
}

uint64_t scheduler_observed_cpu_mask(const scheduler *state) {
  return state ? __atomic_load_n(&state->observed_cpu_mask, __ATOMIC_RELAXED)
               : 0;
}

uint64_t scheduler_work_steals(const scheduler *state) {
  return state ? __atomic_load_n(&state->work_steals, __ATOMIC_RELAXED) : 0;
}
