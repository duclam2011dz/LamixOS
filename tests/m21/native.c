/* SPDX-License-Identifier: GPL-3.0-only */
#include "syscall.h"
#include "scheduler.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

_Static_assert(sizeof(scheduler_frame) == 176, "syscall frame size");
_Static_assert(offsetof(scheduler_frame, rax) == 112, "syscall result offset");
_Static_assert(offsetof(scheduler_frame, rip) == 136, "syscall RIP offset");
_Static_assert(offsetof(scheduler_frame, rsp) == 160, "syscall RSP offset");

typedef struct fixture {
  uint8_t pages[2][4096];
  uint8_t mapped[2];
  uint64_t yielded;
  uint64_t exited;
  uint64_t pointer;
  uint64_t length;
} fixture;

static int read_user(void *context, uint64_t address, uint8_t *target,
                     size_t length) {
  fixture *test = context;
  uint64_t page = address / 4096;
  uint64_t offset = address % 4096;
  if (page >= 2 || !test->mapped[page] || offset + length > 4096)
    return 0;
  memcpy(target, test->pages[page] + offset, length);
  return 1;
}
static int64_t do_yield(void *context) {
  ++((fixture *)context)->yielded;
  return 17;
}
static int64_t do_exit(void *context, uint64_t code) {
  ((fixture *)context)->exited = code;
  return 0;
}
static int64_t do_write(void *context, uint64_t pointer, uint64_t length) {
  fixture *test = context;
  test->pointer = pointer;
  test->length = length;
  return (int64_t)length;
}

int main(void) {
  fixture test = {0};
  lamix_syscall_ops ops = {&test, do_yield, do_exit, do_write};
  assert(lamix_syscall_dispatch(&ops, LAMIX_SYS_VERSION, 0, 0, 0, 0, 0, 0) ==
         INT64_C(0x0000000100000000));
  assert(lamix_syscall_dispatch(&ops, 999, 0, 0, 0, 0, 0, 0) == -LAMIX_ENOSYS);
  assert(lamix_syscall_dispatch(&ops, LAMIX_SYS_YIELD, 0, 0, 0, 0, 0, 0) == 17);
  assert(test.yielded == 1);
  assert(lamix_syscall_dispatch(&ops, LAMIX_SYS_EXIT, 42, 0, 0, 0, 0, 0) == 0);
  assert(test.exited == 42);
  assert(lamix_syscall_dispatch(&ops, LAMIX_SYS_DEBUG_WRITE, 0x1234, 23,
                                0, 0, 0, 0) == 23);
  assert(test.pointer == 0x1234 && test.length == 23);
  assert(!lamix_syscall_handle_valid(0, 16));
  assert(lamix_syscall_handle_valid(1, 16));
  assert(!lamix_syscall_handle_valid(16, 16));

  lamix_user_memory_ops memory = {&test, read_user};
  uint8_t copy[32] = {0};
  test.mapped[0] = test.mapped[1] = 1;
  for (size_t i = 0; i < sizeof(test.pages[0]); ++i)
    test.pages[0][i] = (uint8_t)i;
  memset(test.pages[1], 0xa5, sizeof(test.pages[1]));
  assert(lamix_syscall_copy_from_user(&memory, 4088, copy, sizeof(copy),
                                      8192, 4096));
  assert(copy[0] == (uint8_t)4088 && copy[7] == (uint8_t)4095 &&
         copy[8] == 0xa5 && copy[31] == 0xa5);
  test.mapped[1] = 0;
  memset(copy, 0x5a, sizeof(copy));
  assert(!lamix_syscall_copy_from_user(&memory, 4088, copy, sizeof(copy),
                                       8192, 4096));
  assert(copy[0] == (uint8_t)4088 && copy[8] == 0x5a);
  assert(!lamix_syscall_copy_from_user(&memory, UINT64_MAX - 3, copy, 8,
                                       UINT64_C(0x0000800000000000), 4096));
  assert(!lamix_syscall_copy_from_user(&memory, 8192, copy, 1, 8192, 4096));
  assert(lamix_syscall_copy_from_user(&memory, 8192, 0, 0, 8192, 4096));
  assert(!lamix_syscall_copy_from_user(&memory, 0, copy, 1, 8192, 3000));
  return 0;
}
