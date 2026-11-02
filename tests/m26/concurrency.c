/* SPDX-License-Identifier: GPL-3.0-only */
#include "vfs.h"
#include <assert.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

typedef struct state {
  pthread_mutex_t lock;
  vfs fs;
  vfs_file file;
} state;
static void *a(void *c, size_t n) {
  (void)c;
  return malloc(n);
}
static void d(void *c, void *p) {
  (void)c;
  free(p);
}
static uintptr_t l(void *c) {
  pthread_mutex_lock(&((state *)c)->lock);
  return 0;
}
static void u(void *c, uintptr_t x) {
  (void)x;
  pthread_mutex_unlock(&((state *)c)->lock);
}
static void *writer(void *c) {
  state *s = c;
  char b[16];
  for (unsigned i = 0; i < 1000; i++) {
    memset(b, (int)i, sizeof(b));
    size_t n;
    assert(vfs_write(&s->file, b, sizeof(b), &n) == VFS_OK && n == sizeof(b));
  }
  return 0;
}
int main(void) {
  state s = {0};
  assert(!pthread_mutex_init(&s.lock, 0));
  vfs_memory m = {&s, a, d, l, u};
  tmpfs t;
  vfs_mount mt;
  assert(tmpfs_init(&t, &m, 8) == VFS_OK);
  assert(vfs_init(&s.fs, &m) == VFS_OK);
  assert(vfs_mount_fs(&s.fs, "/", &tmpfs_operations, &t, &mt) == VFS_OK);
  assert(vfs_create(&s.fs, "/race", VFS_FILE) == VFS_OK);
  assert(vfs_open(&s.fs, "/race", 1, &s.file) == VFS_OK);
  pthread_t x, y;
  assert(!pthread_create(&x, 0, writer, &s));
  assert(!pthread_create(&y, 0, writer, &s));
  assert(!pthread_join(x, 0));
  assert(!pthread_join(y, 0));
  assert(vfs_close(&s.file) == VFS_OK);
  vfs_destroy(&s.fs);
  assert(!pthread_mutex_destroy(&s.lock));
  return 0;
}
