/* SPDX-License-Identifier: GPL-3.0-only */
#include <stdint.h>
#include "lamix/syscall.h"

int main(int argc, char **argv, char **envp) {
  static const char ok[] = "M23:APP_BETA:PASS\n";
  static uint64_t zero_initialized;
  if (argc != 2 || argv[1][0] != 'b' || !envp || envp[0][0] != 'M' ||
      zero_initialized != 0)
    return 12;
  (void)lamix_debug_write(ok, sizeof(ok) - 1);
  return 0;
}
