/* SPDX-License-Identifier: GPL-3.0-only */
#include "lamix/syscall.h"

#define USER_CODE UINT64_C(0x0000008000000000)
#define USER_UNMAPPED UINT64_C(0x0000008000500000)

static const char greeting[] = "M21:USER:ABI:PASS\n";
static const char too_long[257] = {0};

__attribute__((noreturn, section(".text.start"))) void _start(void) {
  if (lamix_version() != INT64_C(0x0000000100000000))
    lamix_exit(1);
  if (lamix_debug_write(greeting, sizeof(greeting) - 1) !=
      (int64_t)(sizeof(greeting) - 1))
    lamix_exit(2);
  if (lamix_debug_write(0, 0) != 0 || lamix_debug_write(0, 1) != -3)
    lamix_exit(3);
  if (lamix_debug_write((const void *)(uintptr_t)USER_UNMAPPED, 1) != -3)
    lamix_exit(4);
  if (lamix_debug_write((const void *)(uintptr_t)(UINT64_MAX - 3), 8) != -3)
    lamix_exit(5);
  if (lamix_debug_write((const void *)(uintptr_t)(USER_CODE + 4090), 16) != -3)
    lamix_exit(6);
  if (lamix_debug_write(too_long, sizeof(too_long)) != -4)
    lamix_exit(7);
  if (lamix_syscall6(99, 0, 0, 0, 0, 0, 0) != -2 || lamix_yield() != 0)
    lamix_exit(8);
  __asm__ volatile("std" ::: "cc", "memory");
  if (lamix_yield() != 0)
    lamix_exit(9);
  uint64_t flags;
  __asm__ volatile("pushfq; popq %0" : "=r"(flags) :: "memory");
  if (flags & (UINT64_C(1) << 10))
    lamix_exit(10);
  lamix_exit(0);
  for (;;) __asm__ volatile("pause");
}
