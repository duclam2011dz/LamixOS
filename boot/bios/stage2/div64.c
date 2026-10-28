/* SPDX-License-Identifier: GPL-3.0-only */
#include <stdint.h>

/* Chia nhị phân 64 bước; không dùng / hoặc % để tránh helper gọi lại chính nó.
 * carry giữ bit thứ 65 của remainder, cần cho divisor >= 2^63. */
uint64_t lamix_divmod64(uint64_t n, uint64_t d, uint64_t *remainder) {
  uint64_t q = 0, r = 0;
  if (!d) {
    /* Caller phải kiểm divisor; lỗi nội bộ không được tạo kết quả hợp lệ. */
    __builtin_trap();
  }
  for (unsigned i = 64; i; --i) {
    unsigned carry = (unsigned)(r >> 63);
    r = (r << 1) | ((n >> (i - 1)) & 1u);
    q <<= 1;
    if (carry || r >= d) {
      r -= d;
      q |= 1;
    }
  }
  if (remainder)
    *remainder = r;
  return q;
}

#ifdef LAMIX_I386_RUNTIME
uint64_t __udivdi3(uint64_t n, uint64_t d) {
  return lamix_divmod64(n, d, (void *)0);
}
uint64_t __udivmoddi4(uint64_t n, uint64_t d, uint64_t *r) {
  return lamix_divmod64(n, d, r);
}
uint64_t __umoddi3(uint64_t n, uint64_t d) {
  uint64_t remainder;
  (void)lamix_divmod64(n, d, &remainder);
  return remainder;
}
#endif
