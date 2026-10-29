/* SPDX-License-Identifier: GPL-3.0-only */
#include <stddef.h>
#include <stdint.h>
void *memcpy(void *d, const void *s, size_t n) {
  uint8_t *a = d;
  const uint8_t *b = s;
  while (n--)
    *a++ = *b++;
  return d;
}
void *memset(void *d, int c, size_t n) {
  uint8_t *a = d;
  while (n--)
    *a++ = (uint8_t)c;
  return d;
}
void *memmove(void *d, const void *s, size_t n) {
  uint8_t *a = d;
  const uint8_t *b = s;
  if (a < b)
    while (n--)
      *a++ = *b++;
  else {
    a += n;
    b += n;
    while (n--)
      *--a = *--b;
  }
  return d;
}
int memcmp(const void *a, const void *b, size_t n) {
  const uint8_t *x = a, *y = b;
  while (n--) {
    if (*x != *y)
      return *x < *y ? -1 : 1;
    x++;
    y++;
  }
  return 0;
}
