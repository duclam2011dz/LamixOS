/* SPDX-License-Identifier: GPL-3.0-only */
#include <stdint.h>
#include "lamix/syscall.h"

int main(int argc, char **argv, char **envp) {
#ifdef LAMIX_M24_SMP
  for (volatile uint64_t work = 0; work < UINT64_C(8000000); ++work)
    __asm__ volatile("pause" ::: "memory");
#endif
  static const char ok[] = "M23:APP_ALPHA:PASS\n";
  if (argc != 2 || argv[1][0] != 'a' || !envp || envp[0][0] != 'M')
    return 11;
  (void)lamix_debug_write(ok, sizeof(ok) - 1);
  return 0;
}
