/* SPDX-License-Identifier: GPL-3.0-only */
#include <stdint.h>
/* Oracle constants độc lập; chạy ngay trong core32 để kiểm ABI helpers. */
extern uint64_t __udivdi3(uint64_t, uint64_t);
extern uint64_t __umoddi3(uint64_t, uint64_t);
extern uint64_t __udivmoddi4(uint64_t, uint64_t, uint64_t *);
static int division_check(void) {
  static const uint64_t cases[][4] = {
      {UINT64_C(0), UINT64_C(1), UINT64_C(0), UINT64_C(0)},
      {UINT64_C(1), UINT64_C(1), UINT64_C(1), UINT64_C(0)},
      {UINT64_C(18446744073709551615), UINT64_C(1),
       UINT64_C(18446744073709551615), UINT64_C(0)},
      {UINT64_C(18446744073709551615), UINT64_C(2),
       UINT64_C(9223372036854775807), UINT64_C(1)},
      {UINT64_C(18446744073709551615), UINT64_C(9223372036854775808),
       UINT64_C(1), UINT64_C(9223372036854775807)},
      {UINT64_C(9223372036854775808), UINT64_C(9223372036854775809),
       UINT64_C(0), UINT64_C(9223372036854775808)},
      {UINT64_C(18446744073709551615), UINT64_C(18446744073709551615),
       UINT64_C(1), UINT64_C(0)},
      {UINT64_C(4294967296), UINT64_C(3), UINT64_C(1431655765), UINT64_C(1)},
  };
  for (unsigned i = 0; i < sizeof cases / sizeof cases[0]; i++) {
    uint64_t r;
    const uint64_t *v = cases[i];
    if (__udivdi3(v[0], v[1]) != v[2] || __umoddi3(v[0], v[1]) != v[3] ||
        __udivmoddi4(v[0], v[1], &r) != v[2] || r != v[3])
      return 0;
  }
  return 1;
}
