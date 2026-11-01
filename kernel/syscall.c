/* SPDX-License-Identifier: GPL-3.0-only */
#include "syscall.h"

int64_t lamix_syscall_dispatch(const lamix_syscall_ops *ops, uint64_t number,
                               uint64_t a0, uint64_t a1, uint64_t a2,
                               uint64_t a3, uint64_t a4, uint64_t a5) {
  (void)a1;
  (void)a2;
  (void)a3;
  (void)a4;
  (void)a5;
  switch (number) {
  case LAMIX_SYS_VERSION:
    return (int64_t)(((uint64_t)LAMIX_SYSCALL_ABI_MAJOR << 32) |
                     LAMIX_SYSCALL_ABI_MINOR);
  case LAMIX_SYS_YIELD:
    return ops && ops->yield ? ops->yield(ops->context) : -LAMIX_ESTATE;
  case LAMIX_SYS_EXIT:
    return ops && ops->exit ? ops->exit(ops->context, a0) : -LAMIX_ESTATE;
  case LAMIX_SYS_DEBUG_WRITE:
    return ops && ops->debug_write
               ? ops->debug_write(ops->context, a0, a1)
               : -LAMIX_ESTATE;
#if defined(LAMIX_M22_IPC)
  case LAMIX_SYS_HANDLE_CLOSE:
  case LAMIX_SYS_HANDLE_DUPLICATE:
  case LAMIX_SYS_CHANNEL_CREATE:
  case LAMIX_SYS_CHANNEL_SEND:
  case LAMIX_SYS_CHANNEL_RECEIVE:
  case LAMIX_SYS_SHM_CREATE:
  case LAMIX_SYS_SHM_MAP:
  case LAMIX_SYS_SHM_UNMAP:
    return ops && ops->m22_call
               ? ops->m22_call(ops->context, number, a0, a1, a2, a3, a4, a5)
               : -LAMIX_ESTATE;
#endif
  default:
    return -LAMIX_ENOSYS;
  }
}

int lamix_syscall_handle_valid(uint64_t handle, uint64_t limit) {
  return handle != 0 && handle < limit;
}

int lamix_syscall_copy_from_user(const lamix_user_memory_ops *memory,
                                 uint64_t source, uint8_t *target,
                                 size_t length, uint64_t user_limit,
                                 size_t page_size) {
  if ((!memory || !memory->read) || (!target && length) || !page_size ||
      (page_size & (page_size - 1)))
    return 0;
  if (!length)
    return 1;
  if (source >= user_limit || length > UINT64_MAX - source ||
      source + length > user_limit)
    return 0;
  size_t copied = 0;
  while (copied < length) {
    uint64_t address = source + copied;
    size_t chunk = page_size - (size_t)(address & (page_size - 1));
    if (chunk > length - copied)
      chunk = length - copied;
    if (!memory->read(memory->context, address, target + copied, chunk))
      return 0;
    copied += chunk;
  }
  return 1;
}
