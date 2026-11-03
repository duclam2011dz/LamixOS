/* SPDX-License-Identifier: GPL-3.0-only */
#include "lamix/services.h"
#include "lamix/shell.h"
#include "lamix/syscall.h"
#include "vfs.h"
#include <assert.h>
#include <string.h>

static unsigned char arena[65536];
static size_t arena_used;

static void *test_alloc(void *context, size_t bytes) {
  (void)context;
  size_t aligned = (arena_used + 7u) & ~(size_t)7u;
  if (bytes > sizeof(arena) - aligned)
    return 0;
  void *result = arena + aligned;
  arena_used = aligned + bytes;
  return result;
}
static void test_free(void *context, void *pointer) {
  (void)context;
  (void)pointer;
}

static void test_shell_parser(void) {
  char line[] = "write /data/hello.txt \"hello world\" escaped\\ space";
  char *argv[LAMIX_SHELL_MAX_ARGS + 1];
  size_t argc = 0;
  assert(lamix_shell_parse(line, sizeof(line), argv, &argc) == LAMIX_SHELL_OK);
  assert(argc == 4);
  assert(strcmp(argv[0], "write") == 0);
  assert(strcmp(argv[2], "hello world") == 0);
  assert(strcmp(argv[3], "escaped space") == 0);
  char broken[] = "cat \"unfinished";
  assert(lamix_shell_parse(broken, sizeof(broken), argv, &argc) ==
         LAMIX_SHELL_UNTERMINATED_QUOTE);
  char escaped[] = "cat trailing\\";
  assert(lamix_shell_parse(escaped, sizeof(escaped), argv, &argc) ==
         LAMIX_SHELL_BAD_ESCAPE);
}

static void test_service_graph(void) {
  const lamix_service_spec specs[] = {
      {"console", "/bin/console", 0, 3, 0},
      {"logger", "/bin/logger", 1u << 0, 3, 0},
      {"shell", "/bin/shell", (1u << 0) | (1u << 1), 3,
       (1u << 6) | (1u << 7)}};
  uint8_t order[3];
  assert(lamix_services_order(specs, 3, order) == LAMIX_SERVICES_OK);
  assert(order[0] == 0 && order[1] == 1 && order[2] == 2);
  lamix_service_spec cycle[] = {{"a", "/a", 1u << 1, 3, 0},
                                {"b", "/b", 1u << 0, 3, 0}};
  assert(lamix_services_order(cycle, 2, order) == LAMIX_SERVICES_CYCLE);
  lamix_service_spec missing[] = {{"a", "/a", 1u << 2, 3, 0},
                                  {"b", "/b", 0, 3, 0}};
  assert(lamix_services_order(missing, 2, order) ==
         LAMIX_SERVICES_MISSING_DEPENDENCY);
  lamix_service_runtime runtime = {.state = LAMIX_SERVICE_FAILED};
  uint32_t delay = 0;
  assert(lamix_service_restart_due(&runtime, 3, 10, &delay));
  assert(delay == 1 && runtime.restart_after_tick == 11);
  assert(lamix_service_restart_due(&runtime, 3, 11, &delay));
  assert(delay == 2 && runtime.restart_after_tick == 13);
  assert(lamix_service_restart_due(&runtime, 3, 13, &delay));
  assert(delay == 4 && runtime.restart_after_tick == 17);
  assert(!lamix_service_restart_due(&runtime, 3, 17, &delay));
}

static void test_vfs_listing(void) {
  vfs_memory memory = {.allocate = test_alloc, .deallocate = test_free};
  vfs filesystem;
  tmpfs root;
  vfs_mount mount;
  assert(vfs_init(&filesystem, &memory) == VFS_OK);
  assert(tmpfs_init(&root, &memory, 32) == VFS_OK);
  assert(vfs_mount_fs(&filesystem, "/", &tmpfs_operations, &root, &mount) ==
         VFS_OK);
  assert(vfs_create(&filesystem, "/hello.txt", VFS_FILE) == VFS_OK);
  vfs_dirent entry;
  assert(vfs_list(&filesystem, "/", 0, &entry) == VFS_OK);
  assert(strcmp(entry.name, "hello.txt") == 0 && entry.kind == VFS_FILE);
  assert(vfs_list(&filesystem, "/", 1, &entry) == VFS_NOT_FOUND);
  vfs_destroy(&filesystem);
}

int main(void) {
  test_shell_parser();
  test_service_graph();
  test_vfs_listing();
  return 0;
}
