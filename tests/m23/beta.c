/* SPDX-License-Identifier: GPL-3.0-only */
#include <stdint.h>
#include "lamix/syscall.h"

int main(int argc, char **argv, char **envp) {
#ifdef LAMIX_M24_SMP
  for (volatile uint64_t work = 0; work < UINT64_C(8000000); ++work)
    __asm__ volatile("pause" ::: "memory");
#endif
  static const char ok[] = "M23:APP_BETA:PASS\n";
  static volatile uint8_t bss_probe[4096 + 32];
  if (argc != 2 || argv[1][0] != 'b' || !envp || envp[0][0] != 'M')
    return 12;
  for (uint64_t i = 0; i < sizeof(bss_probe); ++i)
    if (bss_probe[i] != 0)
      return 13;
  (void)lamix_debug_write(ok, sizeof(ok) - 1);
  return 0;
}
