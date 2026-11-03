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
#if defined(LAMIX_M27_PROFILE)
int64_t lamix_file_open(const char *path, uint32_t flags, uint64_t *handle) {
  return lamix_syscall6(LAMIX_SYS_FILE_OPEN, (uintptr_t)path, flags,
                        (uintptr_t)handle, 0, 0, 0);
}
int64_t lamix_file_read(uint64_t handle, void *buffer, uint64_t capacity,
                        uint64_t *length) {
  return lamix_syscall6(LAMIX_SYS_FILE_READ, handle, (uintptr_t)buffer,
                        capacity, (uintptr_t)length, 0, 0);
}
int64_t lamix_file_write(uint64_t handle, const void *buffer, uint64_t length,
                         uint64_t *written) {
  return lamix_syscall6(LAMIX_SYS_FILE_WRITE, handle, (uintptr_t)buffer,
                        length, (uintptr_t)written, 0, 0);
}
int64_t lamix_file_create(const char *path, uint32_t kind) {
  return lamix_syscall6(LAMIX_SYS_FILE_CREATE, (uintptr_t)path, kind, 0, 0, 0, 0);
}
int64_t lamix_file_remove(const char *path) {
  return lamix_syscall6(LAMIX_SYS_FILE_REMOVE, (uintptr_t)path, 0, 0, 0, 0, 0);
}
int64_t lamix_file_rename(const char *from, const char *to) {
  return lamix_syscall6(LAMIX_SYS_FILE_RENAME, (uintptr_t)from,
                        (uintptr_t)to, 0, 0, 0, 0);
}
int64_t lamix_file_list(const char *path, uint64_t index, lamix_dirent *entry) {
  return lamix_syscall6(LAMIX_SYS_FILE_LIST, (uintptr_t)path, index,
                        (uintptr_t)entry, 0, 0, 0);
}
int64_t lamix_file_sync(void) {
  return lamix_syscall6(LAMIX_SYS_FILE_SYNC, 0, 0, 0, 0, 0, 0);
}
int64_t lamix_process_spawn(const char *path, const char *const *argv,
                            uint64_t argc, uint64_t *process_handle) {
  return lamix_process_spawn_with_rights(path, argv, argc, process_handle, 0);
}
int64_t lamix_process_spawn_with_rights(const char *path,
                            const char *const *argv, uint64_t argc,
                            uint64_t *process_handle, uint32_t permissions) {
  return lamix_syscall6(LAMIX_SYS_PROCESS_SPAWN, (uintptr_t)path,
                        (uintptr_t)argv, argc, (uintptr_t)process_handle,
                        permissions, 0);
}
int64_t lamix_process_wait(uint64_t handle, uint64_t *exit_code) {
  return lamix_syscall6(LAMIX_SYS_PROCESS_WAIT, handle, (uintptr_t)exit_code,
                        0, 0, 0, 0);
}
int64_t lamix_process_kill(uint64_t handle) {
  return lamix_syscall6(LAMIX_SYS_PROCESS_KILL, handle, 0, 0, 0, 0, 0);
}
int64_t lamix_service_control(uint32_t action, uint32_t service_id) {
  return lamix_syscall6(LAMIX_SYS_SERVICE_CONTROL, action, service_id,
                        0, 0, 0, 0);
}
int64_t lamix_log_read(uint64_t offset, void *buffer, uint64_t capacity,
                       uint64_t *length) {
  return lamix_syscall6(LAMIX_SYS_LOG_READ, offset, (uintptr_t)buffer,
                        capacity, (uintptr_t)length, 0, 0);
}
int64_t lamix_system_reboot(void) {
  return lamix_syscall6(LAMIX_SYS_SYSTEM_REBOOT, 0, 0, 0, 0, 0, 0);
}
int64_t lamix_console_read(char *character) {
  return lamix_syscall6(LAMIX_SYS_CONSOLE_READ, (uintptr_t)character,
                        0, 0, 0, 0, 0);
}
#endif
