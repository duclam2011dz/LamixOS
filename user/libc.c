/* SPDX-License-Identifier: GPL-3.0-only */
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "lamix/syscall.h"

__attribute__((noreturn)) void exit(int status) {
  (void)lamix_exit((uint64_t)(uint32_t)status);
  for (;;) __asm__ volatile("pause");
}

void *memcpy(void *destination, const void *source, size_t count) {
  unsigned char *d = destination;
  const unsigned char *s = source;
  for (size_t i = 0; i < count; ++i) d[i] = s[i];
  return destination;
}
void *memmove(void *destination, const void *source, size_t count) {
  unsigned char *d = destination;
  const unsigned char *s = source;
  if ((uintptr_t)d < (uintptr_t)s) {
    for (size_t i = 0; i < count; ++i) d[i] = s[i];
  } else if ((uintptr_t)d > (uintptr_t)s) {
    while (count) { --count; d[count] = s[count]; }
  }
  return destination;
}
void *memset(void *destination, int value, size_t count) {
  unsigned char *d = destination;
  for (size_t i = 0; i < count; ++i) d[i] = (unsigned char)value;
  return destination;
}
int memcmp(const void *left, const void *right, size_t count) {
  const unsigned char *a = left, *b = right;
  for (size_t i = 0; i < count; ++i)
    if (a[i] != b[i]) return (int)a[i] - (int)b[i];
  return 0;
}
size_t strlen(const char *s) {
  size_t n = 0;
  while (s[n]) ++n;
  return n;
}
int strcmp(const char *left, const char *right) {
  const unsigned char *a = (const unsigned char *)left;
  const unsigned char *b = (const unsigned char *)right;
  while (*a && *a == *b) { ++a; ++b; }
  return (int)*a - (int)*b;
}
