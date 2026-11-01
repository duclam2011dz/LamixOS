/* SPDX-License-Identifier: GPL-3.0-only */
#include "lamix/syscall.h"
#include <stdlib.h>

extern int main(int argc, char **argv, char **envp);

__attribute__((noreturn)) void lamix_crt_start(uintptr_t *stack) {
  int argc = (int)stack[0];
  char **argv = (char **)&stack[1];
  char **envp = argv + argc + 1;
  exit(main(argc, argv, envp));
  for (;;) __asm__ volatile("pause");
}
