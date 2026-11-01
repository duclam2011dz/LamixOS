/* SPDX-License-Identifier: GPL-3.0-only */
#include "lamix/syscall.h"

__attribute__((noreturn, section(".text.start"))) void _start(void) {
  volatile uint64_t *progress =
      (volatile uint64_t *)(uintptr_t)UINT64_C(0x0000008000100000);
  for (;;) {
    ++*progress;
    (void)lamix_yield();
  }
}
