/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef LAMIX_USER_SYSCALL_H
#define LAMIX_USER_SYSCALL_H

#include <stdint.h>

#define LAMIX_ABI_VERSION_MAJOR 1u
#define LAMIX_ABI_VERSION_MINOR 0u
#define LAMIX_ABI_MAX_WRITE 256u

enum lamix_syscall_number {
  LAMIX_SYS_VERSION = 0,
  LAMIX_SYS_YIELD = 1,
  LAMIX_SYS_EXIT = 2,
  LAMIX_SYS_DEBUG_WRITE = 3
};

int64_t lamix_syscall6(uint64_t number, uint64_t a0, uint64_t a1,
                       uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5);
static inline int64_t lamix_version(void) {
  return lamix_syscall6(LAMIX_SYS_VERSION, 0, 0, 0, 0, 0, 0);
}
static inline int64_t lamix_yield(void) {
  return lamix_syscall6(LAMIX_SYS_YIELD, 0, 0, 0, 0, 0, 0);
}
static inline int64_t lamix_exit(uint64_t code) {
  return lamix_syscall6(LAMIX_SYS_EXIT, code, 0, 0, 0, 0, 0);
}
static inline int64_t lamix_debug_write(const void *bytes, uint64_t length) {
  return lamix_syscall6(LAMIX_SYS_DEBUG_WRITE, (uintptr_t)bytes, length,
                        0, 0, 0, 0);
}

#endif
