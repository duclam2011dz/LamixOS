/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef LAMIX_USER_SYSCALL_H
#define LAMIX_USER_SYSCALL_H

#include <stdint.h>

#define LAMIX_ABI_VERSION_MAJOR 1u
#if defined(LAMIX_M22_PROFILE)
#define LAMIX_ABI_VERSION_MINOR 1u
#else
#define LAMIX_ABI_VERSION_MINOR 0u
#endif
#define LAMIX_ABI_MAX_WRITE 256u

enum lamix_syscall_number {
  LAMIX_SYS_VERSION = 0,
  LAMIX_SYS_YIELD = 1,
  LAMIX_SYS_EXIT = 2,
  LAMIX_SYS_DEBUG_WRITE = 3
#if defined(LAMIX_M22_PROFILE)
  ,
  LAMIX_SYS_HANDLE_CLOSE = 4,
  LAMIX_SYS_HANDLE_DUPLICATE = 5,
  LAMIX_SYS_CHANNEL_CREATE = 6,
  LAMIX_SYS_CHANNEL_SEND = 7,
  LAMIX_SYS_CHANNEL_RECEIVE = 8,
  LAMIX_SYS_SHM_CREATE = 9,
  LAMIX_SYS_SHM_MAP = 10,
  LAMIX_SYS_SHM_UNMAP = 11
#endif
};
#if defined(LAMIX_M22_PROFILE)
enum lamix_error {
  LAMIX_EINVAL=1, LAMIX_ENOSYS=2, LAMIX_EFAULT=3, LAMIX_E2BIG=4,
  LAMIX_ESTATE=5, LAMIX_EBADF=6, LAMIX_EACCES=7, LAMIX_EAGAIN=8,
  LAMIX_EMSGSIZE=9, LAMIX_EMFILE=10, LAMIX_ECLOSED=11, LAMIX_ENOMEM=12,
  LAMIX_EEXIST=13
};
enum lamix_rights {
  LAMIX_RIGHT_TRANSFER=1u<<0, LAMIX_RIGHT_CHANNEL_SEND=1u<<1,
  LAMIX_RIGHT_CHANNEL_RECEIVE=1u<<2, LAMIX_RIGHT_SHM_MAP=1u<<3,
  LAMIX_RIGHT_SHM_READ=1u<<4, LAMIX_RIGHT_SHM_WRITE=1u<<5
};
enum lamix_attachment_disposition {
  LAMIX_ATTACHMENT_MOVE=1, LAMIX_ATTACHMENT_DUP=2
};
typedef struct lamix_attachment {
  uint64_t handle; uint32_t rights; uint8_t disposition; uint8_t reserved[3];
} lamix_attachment;
_Static_assert(sizeof(lamix_attachment)==16,"M22 attachment ABI");
#endif

int64_t lamix_syscall6(uint64_t number, uint64_t a0, uint64_t a1,
                       uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5);
static inline int64_t lamix_version(void) {
  return lamix_syscall6(LAMIX_SYS_VERSION, 0, 0, 0, 0, 0, 0);
}
static inline int64_t lamix_yield(void) {
  return lamix_syscall6(LAMIX_SYS_YIELD, 0, 0, 0, 0, 0, 0);
}
static inline int64_t lamix_exit(uint64_t code) {
  return lamix_syscall6(LAMIX_SYS_EXIT, code, 0, 0, 0, 0, 0);
}
static inline int64_t lamix_debug_write(const void *bytes, uint64_t length) {
  return lamix_syscall6(LAMIX_SYS_DEBUG_WRITE, (uintptr_t)bytes, length,
                        0, 0, 0, 0);
}
#if defined(LAMIX_M22_PROFILE)
int64_t lamix_handle_close(uint64_t handle);
int64_t lamix_handle_duplicate(uint64_t handle,uint32_t rights,uint64_t *out);
int64_t lamix_channel_create(uint64_t *a,uint64_t *b);
int64_t lamix_channel_send(uint64_t endpoint,const void *data,uint64_t length,
                           const lamix_attachment *attachments,uint64_t count);
int64_t lamix_channel_receive(uint64_t endpoint,void *data,uint64_t capacity,
                              uint64_t *length,lamix_attachment *attachments,
                              uint64_t *attachment_count);
int64_t lamix_shm_create(uint64_t pages,uint64_t *handle);
int64_t lamix_shm_map(uint64_t handle,uint64_t address,uint32_t rights);
int64_t lamix_shm_unmap(uint64_t address);
#endif

#endif
