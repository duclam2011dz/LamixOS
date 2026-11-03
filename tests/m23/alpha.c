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
#ifdef LAMIX_M27_PROFILE
  uint64_t denied_handle = 0;
  if (lamix_file_open("/data/M27-SHELL.TXT", 1, &denied_handle) !=
      -LAMIX_EACCES)
    return 13;
  static const char denied[] = "M27:CAPABILITY:DENY:PASS\n";
  if (lamix_debug_write(denied, sizeof(denied) - 1) !=
      (int64_t)(sizeof(denied) - 1)) return 15;
#endif
  if (lamix_debug_write(ok, sizeof(ok) - 1) != (int64_t)(sizeof(ok) - 1))
    return 14;
  return 0;
}
