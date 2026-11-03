/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef LAMIX_KERNEL_SYSCALL_H
#define LAMIX_KERNEL_SYSCALL_H

#include <stdint.h>
#include <stddef.h>

#define LAMIX_SYSCALL_ABI_MAJOR 1u
#if defined(LAMIX_M27_PROFILE)
#define LAMIX_SYSCALL_ABI_MINOR 2u
#elif defined(LAMIX_M22_IPC)
#define LAMIX_SYSCALL_ABI_MINOR 1u
#else
#define LAMIX_SYSCALL_ABI_MINOR 0u
#endif
#define LAMIX_SYSCALL_MAX_WRITE 256u

typedef enum lamix_syscall_number {
  LAMIX_SYS_VERSION = 0,
  LAMIX_SYS_YIELD = 1,
  LAMIX_SYS_EXIT = 2,
  LAMIX_SYS_DEBUG_WRITE = 3,
#if defined(LAMIX_M22_IPC)
  LAMIX_SYS_HANDLE_CLOSE = 4,
  LAMIX_SYS_HANDLE_DUPLICATE = 5,
  LAMIX_SYS_CHANNEL_CREATE = 6,
  LAMIX_SYS_CHANNEL_SEND = 7,
  LAMIX_SYS_CHANNEL_RECEIVE = 8,
  LAMIX_SYS_SHM_CREATE = 9,
  LAMIX_SYS_SHM_MAP = 10,
  LAMIX_SYS_SHM_UNMAP = 11,
#if defined(LAMIX_M27_PROFILE)
  LAMIX_SYS_FILE_OPEN = 12,
  LAMIX_SYS_FILE_READ = 13,
  LAMIX_SYS_FILE_WRITE = 14,
  LAMIX_SYS_FILE_CREATE = 15,
  LAMIX_SYS_FILE_REMOVE = 16,
  LAMIX_SYS_FILE_RENAME = 17,
  LAMIX_SYS_FILE_LIST = 18,
  LAMIX_SYS_FILE_SYNC = 19,
  LAMIX_SYS_PROCESS_SPAWN = 20,
  LAMIX_SYS_PROCESS_WAIT = 21,
  LAMIX_SYS_PROCESS_KILL = 22,
  LAMIX_SYS_SERVICE_CONTROL = 23,
  LAMIX_SYS_LOG_READ = 24,
  LAMIX_SYS_SYSTEM_REBOOT = 25,
  LAMIX_SYS_CONSOLE_READ = 26,
  LAMIX_SYS_COUNT = 27
#else
  LAMIX_SYS_COUNT = 12
#endif
#else
  LAMIX_SYS_COUNT = 4
#endif
} lamix_syscall_number;

typedef enum lamix_syscall_error {
  LAMIX_EINVAL = 1,
  LAMIX_ENOSYS = 2,
  LAMIX_EFAULT = 3,
  LAMIX_E2BIG = 4,
  LAMIX_ESTATE = 5
#if defined(LAMIX_M22_IPC)
  ,
  LAMIX_EBADF = 6,
  LAMIX_EACCES = 7,
  LAMIX_EAGAIN = 8,
  LAMIX_EMSGSIZE = 9,
  LAMIX_EMFILE = 10,
  LAMIX_ECLOSED = 11,
  LAMIX_ENOMEM = 12,
  LAMIX_EEXIST = 13
#if defined(LAMIX_M27_PROFILE)
  , LAMIX_ENOTDIR = 14, LAMIX_EISDIR = 15, LAMIX_EROFS = 16,
  LAMIX_EIO = 17, LAMIX_ECORRUPT = 18, LAMIX_EBUSY = 19,
  LAMIX_ECHILD = 20, LAMIX_ENOENT = 21
#endif
#endif
} lamix_syscall_error;

typedef struct lamix_syscall_ops {
  void *context;
  int64_t (*yield)(void *context);
  int64_t (*exit)(void *context, uint64_t code);
  int64_t (*debug_write)(void *context, uint64_t pointer, uint64_t length);
#if defined(LAMIX_M22_IPC)
  int64_t (*m22_call)(void *context, uint64_t number, uint64_t a0,
                      uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4,
                      uint64_t a5);
#endif
} lamix_syscall_ops;

typedef struct lamix_user_memory_ops {
  void *context;
  int (*read)(void *context, uint64_t address, uint8_t *target, size_t length);
} lamix_user_memory_ops;

/* Syscall arguments are RDI, RSI, RDX, R10, R8, R9. */
int64_t lamix_syscall_dispatch(const lamix_syscall_ops *ops, uint64_t number,
                               uint64_t a0, uint64_t a1, uint64_t a2,
                               uint64_t a3, uint64_t a4, uint64_t a5);
int lamix_syscall_handle_valid(uint64_t handle, uint64_t limit);
int lamix_syscall_copy_from_user(const lamix_user_memory_ops *memory,
                                 uint64_t source, uint8_t *target,
                                 size_t length, uint64_t user_limit,
                                 size_t page_size);

#endif
