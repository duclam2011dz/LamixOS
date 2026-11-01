/* SPDX-License-Identifier: GPL-3.0-only */
#include "lamix/syscall.h"

int64_t lamix_syscall6(uint64_t number, uint64_t a0, uint64_t a1,
                       uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5) {
  register uint64_t r10 __asm__("r10") = a3;
  register uint64_t r8 __asm__("r8") = a4;
  register uint64_t r9 __asm__("r9") = a5;
  register uint64_t rax __asm__("rax") = number;
  __asm__ volatile("syscall"
                   : "+a"(rax)
                   : "D"(a0), "S"(a1), "d"(a2), "r"(r10), "r"(r8),
                     "r"(r9)
                   : "rcx", "r11", "memory");
  return (int64_t)rax;
}

#if defined(LAMIX_M22_PROFILE)
int64_t lamix_handle_close(uint64_t handle) {
  return lamix_syscall6(LAMIX_SYS_HANDLE_CLOSE,handle,0,0,0,0,0);
}
int64_t lamix_handle_duplicate(uint64_t handle,uint32_t rights,uint64_t *out) {
  return lamix_syscall6(LAMIX_SYS_HANDLE_DUPLICATE,handle,rights,(uintptr_t)out,0,0,0);
}
int64_t lamix_channel_create(uint64_t *a,uint64_t *b) {
  return lamix_syscall6(LAMIX_SYS_CHANNEL_CREATE,(uintptr_t)a,(uintptr_t)b,0,0,0,0);
}
int64_t lamix_channel_send(uint64_t endpoint,const void *data,uint64_t length,
                           const lamix_attachment *attachments,uint64_t count) {
  return lamix_syscall6(LAMIX_SYS_CHANNEL_SEND,endpoint,(uintptr_t)data,length,
                        (uintptr_t)attachments,count,0);
}
int64_t lamix_channel_receive(uint64_t endpoint,void *data,uint64_t capacity,
                              uint64_t *length,lamix_attachment *attachments,
                              uint64_t *attachment_count) {
  return lamix_syscall6(LAMIX_SYS_CHANNEL_RECEIVE,endpoint,(uintptr_t)data,
                        capacity,(uintptr_t)length,(uintptr_t)attachments,
                        (uintptr_t)attachment_count);
}
int64_t lamix_shm_create(uint64_t pages,uint64_t *handle) {
  return lamix_syscall6(LAMIX_SYS_SHM_CREATE,pages,(uintptr_t)handle,0,0,0,0);
}
int64_t lamix_shm_map(uint64_t handle,uint64_t address,uint32_t rights) {
  return lamix_syscall6(LAMIX_SYS_SHM_MAP,handle,address,rights,0,0,0);
}
int64_t lamix_shm_unmap(uint64_t address) {
  return lamix_syscall6(LAMIX_SYS_SHM_UNMAP,address,0,0,0,0,0);
}
#endif
