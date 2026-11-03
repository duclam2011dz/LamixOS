/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef LAMIX_USER_SYSCALL_H
#define LAMIX_USER_SYSCALL_H

#include <stdint.h>

#define LAMIX_ABI_VERSION_MAJOR 1u
#if defined(LAMIX_M27_PROFILE)
#define LAMIX_ABI_VERSION_MINOR 2u
#elif defined(LAMIX_M22_PROFILE)
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
#if defined(LAMIX_M27_PROFILE)
enum lamix_file_flags { LAMIX_FILE_READ=1u, LAMIX_FILE_WRITE=2u };
enum lamix_file_kind { LAMIX_FILE_REGULAR=0u, LAMIX_FILE_DIRECTORY=1u };
typedef struct lamix_dirent { char name[256]; uint32_t kind; uint32_t reserved;
  uint64_t size; } lamix_dirent;
enum lamix_syscall_number_m27 {
  LAMIX_SYS_FILE_OPEN=12, LAMIX_SYS_FILE_READ=13, LAMIX_SYS_FILE_WRITE=14,
  LAMIX_SYS_FILE_CREATE=15, LAMIX_SYS_FILE_REMOVE=16,
  LAMIX_SYS_FILE_RENAME=17, LAMIX_SYS_FILE_LIST=18, LAMIX_SYS_FILE_SYNC=19,
  LAMIX_SYS_PROCESS_SPAWN=20, LAMIX_SYS_PROCESS_WAIT=21,
  LAMIX_SYS_PROCESS_KILL=22, LAMIX_SYS_SERVICE_CONTROL=23,
  LAMIX_SYS_LOG_READ=24, LAMIX_SYS_SYSTEM_REBOOT=25,
  LAMIX_SYS_CONSOLE_READ=26
};
enum lamix_error_m27 { LAMIX_ENOTDIR=14, LAMIX_EISDIR=15, LAMIX_EROFS=16,
  LAMIX_EIO=17, LAMIX_ECORRUPT=18, LAMIX_EBUSY=19, LAMIX_ECHILD=20,
  LAMIX_ENOENT=21 };
enum lamix_rights_m27 { LAMIX_RIGHT_FILE_READ=1u<<6,
  LAMIX_RIGHT_FILE_WRITE=1u<<7, LAMIX_RIGHT_FILE_ENUM=1u<<8,
  LAMIX_RIGHT_SERVICE_CONTROL=1u<<9, LAMIX_RIGHT_SYSTEM_REBOOT=1u<<10,
  LAMIX_RIGHT_LOG_READ=1u<<11, LAMIX_RIGHT_PROCESS_WAIT=1u<<12,
  LAMIX_RIGHT_PROCESS_KILL=1u<<13, LAMIX_RIGHT_PROCESS_SPAWN=1u<<14 };
#endif
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
#if defined(LAMIX_M27_PROFILE)
int64_t lamix_file_open(const char *path, uint32_t flags, uint64_t *handle);
int64_t lamix_file_read(uint64_t handle, void *buffer, uint64_t capacity,
                        uint64_t *length);
int64_t lamix_file_write(uint64_t handle, const void *buffer, uint64_t length,
                         uint64_t *written);
int64_t lamix_file_create(const char *path, uint32_t kind);
int64_t lamix_file_remove(const char *path);
int64_t lamix_file_rename(const char *from, const char *to);
int64_t lamix_file_list(const char *path, uint64_t index, lamix_dirent *entry);
int64_t lamix_file_sync(void);
int64_t lamix_process_spawn(const char *path, const char *const *argv,
                            uint64_t argc, uint64_t *process_handle);
int64_t lamix_process_spawn_with_rights(const char *path,
                            const char *const *argv, uint64_t argc,
                            uint64_t *process_handle, uint32_t permissions);
int64_t lamix_process_wait(uint64_t process_handle, uint64_t *exit_code);
int64_t lamix_process_kill(uint64_t process_handle);
int64_t lamix_service_control(uint32_t action, uint32_t service_id);
int64_t lamix_log_read(uint64_t offset, void *buffer, uint64_t capacity,
                       uint64_t *length);
int64_t lamix_system_reboot(void);
int64_t lamix_console_read(char *character);
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
