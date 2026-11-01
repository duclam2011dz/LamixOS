/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef LAMIX_KERNEL_PROCESS_H
#define LAMIX_KERNEL_PROCESS_H

#include "scheduler.h"
#include "vmm.h"
#if defined(LAMIX_M23_ELF)
#include "elf_loader.h"
#endif
#if defined(LAMIX_M22_IPC)
#include "handle.h"
#include "ipc.h"
#endif

typedef enum process_state {
  PROCESS_EMPTY,
  PROCESS_READY,
  PROCESS_RUNNING,
  PROCESS_FAULTED,
  PROCESS_KILLED,
  PROCESS_EXITED
} process_state;

typedef enum process_status {
  PROCESS_OK,
  PROCESS_INVALID,
  PROCESS_OUT_OF_MEMORY,
  PROCESS_VM_ERROR,
  PROCESS_SCHEDULER_ERROR,
  PROCESS_BUSY,
  PROCESS_RELEASE_ERROR
} process_status;

typedef struct process_operations {
  void *context;
  int (*allocate_frame)(void *context, uint64_t *physical);
  int (*release_frame)(void *context, uint64_t physical);
  volatile uint8_t *(*access_frame)(void *context, uint64_t physical);
  vmm_operations vmm;
} process_operations;

typedef struct process {
  process_operations operations;
  vmm_space address_space;
#if defined(LAMIX_M22_IPC)
  lamix_handle_table handles;
  lamix_shm_mapping *shm_mappings;
#endif
  scheduler *scheduler_state;
  scheduler_thread *thread;
  uint64_t id;
  uint64_t code_frame, stack_frame, data_frame;
  uint64_t fault_address, fault_code;
  uint64_t exit_code;
#if defined(LAMIX_M23_ELF)
  struct process_user_page *user_pages;
  size_t user_page_count;
#endif
  uint32_t creation_stage, creation_vm_status;
  volatile uint8_t kill_requested;
  volatile process_state state;
} process;

#if defined(LAMIX_M23_ELF)
typedef struct process_user_page {
  uint64_t address;
  uint64_t physical;
} process_user_page;
#endif

#define PROCESS_USER_CODE UINT64_C(0x0000008000000000)
#define PROCESS_USER_DATA UINT64_C(0x0000008000100000)
#define PROCESS_USER_GUARD UINT64_C(0x00000080002ff000)
#define PROCESS_USER_STACK UINT64_C(0x0000008000300000)
#define PROCESS_USER_INVALID UINT64_C(0x0000008000500000)
#define PROCESS_USER_PRIVATE UINT64_C(0x0000008000600000)

process_status process_create(process *instance,
                              const process_operations *operations,
                              const vmm_space *kernel_space,
                              scheduler *scheduler_state, uint64_t id,
                              const void *image, size_t image_size,
                              uint8_t priority);
#if defined(LAMIX_M23_ELF)
process_status process_create_elf(process *instance,
                                  const process_operations *operations,
                                  const vmm_space *kernel_space,
                                  scheduler *scheduler_state, uint64_t id,
                                  const void *image, size_t image_size,
                                  const char *const *argv, size_t argc,
                                  const char *const *envp, size_t envc,
                                  uint8_t priority);
#endif
void process_note_fault(process *instance, uint64_t address, uint64_t code);
void process_note_exit(process *instance, uint64_t code);
process_status process_request_kill(process *instance);
process_state process_wait(const process *instance, uint64_t *fault_address,
                           uint64_t *fault_code);
process_status process_destroy(process *instance);

#endif
