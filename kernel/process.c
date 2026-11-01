/* SPDX-License-Identifier: GPL-3.0-only */
#include "process.h"

static void clear_bytes(volatile uint8_t *memory, size_t length) {
  for (size_t i = 0; i < length; ++i)
    memory[i] = 0;
}

static int allocate_user_frame(process *instance, uint64_t *frame) {
  if (!instance->operations.allocate_frame(instance->operations.context,
                                            frame))
    return 0;
  volatile uint8_t *memory = instance->operations.access_frame(
      instance->operations.context, *frame);
  if (!memory) {
    (void)instance->operations.release_frame(instance->operations.context,
                                              *frame);
    *frame = 0;
    return 0;
  }
  clear_bytes(memory, VMM_PAGE_SIZE);
  return 1;
}

static void process_entry_unused(void *argument) { (void)argument; }

#if defined(LAMIX_M22_IPC)
static void *process_alloc(void *context, size_t size) {
  process *p = context;
  return p->scheduler_state->operations.allocate(
      p->scheduler_state->operations.context, size);
}
static void process_dealloc(void *context, void *pointer) {
  process *p = context;
  p->scheduler_state->operations.deallocate(
      p->scheduler_state->operations.context, pointer);
}
static uintptr_t process_lock(void *context) {
  process *p = context;
  return p->scheduler_state->operations.irq_save(
      p->scheduler_state->operations.context);
}
static void process_unlock(void *context, uintptr_t flags) {
  process *p = context;
  p->scheduler_state->operations.irq_restore(
      p->scheduler_state->operations.context, flags);
}
#endif

static int release_owned_frames(process *instance) {
  uint64_t *frames[] = {&instance->data_frame, &instance->stack_frame,
                        &instance->code_frame};
  for (unsigned i = 0; i < sizeof(frames) / sizeof(frames[0]); ++i) {
    if (*frames[i] && !instance->operations.release_frame(
                          instance->operations.context, *frames[i]))
      return 0;
    *frames[i] = 0;
  }
  return 1;
}

process_status process_create(process *instance,
                              const process_operations *operations,
                              const vmm_space *kernel_space,
                              scheduler *scheduler_state, uint64_t id,
                              const void *image, size_t image_size,
                              uint8_t priority) {
  if (!instance || !operations || !operations->allocate_frame ||
      !operations->release_frame || !operations->access_frame ||
      !operations->vmm.allocate_page || !operations->vmm.release_page ||
      !operations->vmm.access_page || !operations->vmm.invalidate ||
      !kernel_space || !kernel_space->nx_enabled || !scheduler_state || !id || !image || !image_size ||
      image_size > VMM_PAGE_SIZE || priority > SCHED_PRIORITY_MAX)
    return PROCESS_INVALID;
  *instance = (process){.operations = *operations,
                        .scheduler_state = scheduler_state, .id = id};
#if defined(LAMIX_M22_IPC)
  if (lamix_handle_table_init(&instance->handles, instance, process_alloc,
                              process_dealloc, process_lock,
                              process_unlock) != LH_OK)
    return PROCESS_OUT_OF_MEMORY;
#endif
  if (vmm_init(&instance->address_space, &operations->vmm,
               kernel_space->nx_enabled) != VMM_OK) {
#if defined(LAMIX_M22_IPC)
    lamix_handle_table_destroy(&instance->handles);
#endif
    return PROCESS_OUT_OF_MEMORY;
  }
  instance->creation_stage = 1;
  vmm_status vm = vmm_copy_kernel_mappings(&instance->address_space,
                                            kernel_space);
  instance->creation_vm_status = (uint32_t)vm;
  if (vm != VMM_OK) {
    instance->creation_stage = 2;
    goto vm_failure;
  }
  instance->creation_stage = 3;
  if (!allocate_user_frame(instance, &instance->code_frame) ||
      !allocate_user_frame(instance, &instance->stack_frame) ||
      !allocate_user_frame(instance, &instance->data_frame))
    goto memory_failure;
  volatile uint8_t *code = operations->access_frame(operations->context,
                                                    instance->code_frame);
  if (!code)
    goto memory_failure;
  const uint8_t *source = (const uint8_t *)image;
  for (size_t i = 0; i < image_size; ++i)
    code[i] = source[i];
  instance->creation_stage = 4;
  vm = vmm_map_range(&instance->address_space, PROCESS_USER_CODE,
                     instance->code_frame, VMM_PAGE_SIZE,
                     VMM_READ | VMM_EXECUTE | VMM_USER, 0);
  if (vm == VMM_OK)
    vm = vmm_map_range(&instance->address_space, PROCESS_USER_DATA,
                       instance->data_frame, VMM_PAGE_SIZE,
                       VMM_READ | VMM_WRITE | VMM_USER, 0);
  if (vm == VMM_OK)
    vm = vmm_map_range(&instance->address_space, PROCESS_USER_STACK,
                       instance->stack_frame, VMM_PAGE_SIZE,
                       VMM_READ | VMM_WRITE | VMM_USER, 0);
  instance->creation_vm_status = (uint32_t)vm;
  if (vm != VMM_OK)
    goto vm_failure;
  instance->creation_stage = 5;
  if (scheduler_create(scheduler_state, process_entry_unused, instance,
                       priority, &instance->thread) != SCHED_OK)
    goto memory_failure;
  if (scheduler_configure_user(instance->thread, PROCESS_USER_CODE,
                               PROCESS_USER_STACK + VMM_PAGE_SIZE - 16) !=
      SCHED_OK) {
    /* Thread storage is scheduler-owned; make creation failure fatal to caller. */
    return PROCESS_SCHEDULER_ERROR;
  }
  instance->thread->private_context = instance;
  instance->state = PROCESS_READY;
  return PROCESS_OK;

memory_failure:
#if defined(LAMIX_M22_IPC)
  lamix_handle_table_destroy(&instance->handles);
#endif
  (void)release_owned_frames(instance);
  (void)vmm_destroy_process_space(&instance->address_space);
  return PROCESS_OUT_OF_MEMORY;
vm_failure:
#if defined(LAMIX_M22_IPC)
  lamix_handle_table_destroy(&instance->handles);
#endif
  (void)release_owned_frames(instance);
  (void)vmm_destroy_process_space(&instance->address_space);
  return PROCESS_VM_ERROR;
}

void process_note_fault(process *instance, uint64_t address, uint64_t code) {
  if (!instance || instance->state == PROCESS_FAULTED ||
      instance->state == PROCESS_KILLED || instance->state == PROCESS_EXITED)
    return;
  instance->fault_address = address;
  instance->fault_code = code;
  instance->state = PROCESS_FAULTED;
}

void process_note_exit(process *instance, uint64_t code) {
  if (!instance || instance->state == PROCESS_FAULTED ||
      instance->state == PROCESS_KILLED || instance->state == PROCESS_EXITED)
    return;
  instance->exit_code = code;
  instance->state = PROCESS_EXITED;
}

process_status process_request_kill(process *instance) {
  if (!instance || instance->state == PROCESS_EMPTY)
    return PROCESS_INVALID;
  if (instance->state == PROCESS_FAULTED || instance->state == PROCESS_KILLED ||
      instance->state == PROCESS_EXITED)
    return PROCESS_OK;
  instance->kill_requested = 1;
  return PROCESS_OK;
}

process_state process_wait(const process *instance, uint64_t *fault_address,
                           uint64_t *fault_code) {
  if (!instance)
    return PROCESS_EMPTY;
  while ((instance->state == PROCESS_READY ||
          instance->state == PROCESS_RUNNING) && instance->scheduler_state &&
         instance->scheduler_state->active) {
    if (scheduler_yield(instance->scheduler_state) != SCHED_OK)
      break;
  }
  if (fault_address)
    *fault_address = instance->fault_address;
  if (fault_code)
    *fault_code = instance->fault_code;
  return instance->state;
}

process_status process_destroy(process *instance) {
  if (!instance || instance->state == PROCESS_EMPTY)
    return PROCESS_INVALID;
  if (instance->state != PROCESS_FAULTED && instance->state != PROCESS_KILLED &&
      instance->state != PROCESS_EXITED)
    return PROCESS_BUSY;
#if defined(LAMIX_M22_IPC)
  while (instance->shm_mappings) {
    lamix_shm_mapping *mapping=instance->shm_mappings;
    if (vmm_unmap_range(&instance->address_space,mapping->address,
                        (uint64_t)mapping->pages*VMM_PAGE_SIZE)!=VMM_OK)
      return PROCESS_VM_ERROR;
    instance->shm_mappings=mapping->next;
    lamix_object_release(mapping->object);
    instance->scheduler_state->operations.deallocate(
        instance->scheduler_state->operations.context,mapping);
  }
  lamix_handle_table_destroy(&instance->handles);
#endif
  if (instance->address_space.initialized) {
    vmm_status status = vmm_destroy_process_space(&instance->address_space);
    if (status != VMM_OK)
      return PROCESS_VM_ERROR;
  }
  if (!release_owned_frames(instance))
    return PROCESS_VM_ERROR;
  instance->state = PROCESS_EMPTY;
  return PROCESS_OK;
}
