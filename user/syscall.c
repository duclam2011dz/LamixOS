/* SPDX-License-Identifier: GPL-3.0-only */
#include "lamix/syscall.h"

int64_t lamix_syscall6(uint64_t number, uint64_t a0, uint64_t a1,
                       uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5) {
  register uint64_t r10 __asm__("r10") = a3;
  register uint64_t r8 __asm__("r8") = a4;
  register uint64_t r9 __asm__("r9") = a5;
  register uint64_t rax __asm__("rax") = number;
  __asm__ volatile("syscall"
                   : "+a"(rax)
                   : "D"(a0), "S"(a1), "d"(a2), "r"(r10), "r"(r8),
                     "r"(r9)
                   : "rcx", "r11", "memory");
  return (int64_t)rax;
}
