/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef LAMIX_KERNEL_VFS_H
#define LAMIX_KERNEL_VFS_H

#include <stddef.h>
#include <stdint.h>

typedef enum vfs_status {
  VFS_OK,
  VFS_INVALID,
  VFS_NOT_FOUND,
  VFS_EXISTS,
  VFS_NOT_DIRECTORY,
  VFS_IS_DIRECTORY,
  VFS_READ_ONLY,
  VFS_NO_MEMORY,
  VFS_IO_ERROR,
  VFS_CORRUPT,
  VFS_BUSY,
  VFS_TOO_LARGE,
  VFS_UNSUPPORTED
} vfs_status;

typedef enum vfs_kind { VFS_FILE, VFS_DIRECTORY } vfs_kind;
typedef struct vfs_dirent {
  char name[256];
  vfs_kind kind;
  uint64_t size;
} vfs_dirent;
typedef struct vfs_node vfs_node;
typedef struct vfs_mount vfs_mount;
typedef struct vfs_ops {
  vfs_status (*lookup)(void *, const char *, vfs_node *);
  vfs_status (*create)(void *, const char *, vfs_kind);
  vfs_status (*read)(void *, const char *, uint64_t, void *, size_t, size_t *);
  vfs_status (*write)(void *, const char *, uint64_t, const void *, size_t,
                      size_t *);
  vfs_status (*remove)(void *, const char *);
  vfs_status (*rename)(void *, const char *, const char *);
  vfs_status (*sync)(void *);
  void (*destroy)(void *);
  vfs_status (*list)(void *, const char *, size_t, vfs_dirent *);
} vfs_ops;

typedef struct vfs_memory {
  void *context;
  void *(*allocate)(void *, size_t);
  void (*deallocate)(void *, void *);
  uintptr_t (*lock)(void *);
  void (*unlock)(void *, uintptr_t);
} vfs_memory;

struct vfs_node {
  vfs_kind kind;
  uint64_t size;
  uint8_t read_only;
};
struct vfs_mount {
  vfs_mount *next;
  char path[128];
  const vfs_ops *ops;
  void *context;
  size_t open_files;
  vfs_memory memory;
  uint8_t mounted;
};
typedef struct vfs {
  vfs_memory memory;
  vfs_mount *mounts;
} vfs;
typedef struct vfs_file {
  vfs_mount *mount;
  char path[256];
  uint64_t offset;
  uint8_t writable;
} vfs_file;

vfs_status vfs_init(vfs *, const vfs_memory *);
vfs_status vfs_mount_fs(vfs *, const char *, const vfs_ops *, void *,
                        vfs_mount *);
vfs_status vfs_unmount(vfs *, const char *);
vfs_status vfs_lookup(vfs *, const char *, vfs_node *);
vfs_status vfs_list(vfs *, const char *, size_t, vfs_dirent *);
vfs_status vfs_create(vfs *, const char *, vfs_kind);
vfs_status vfs_open(vfs *, const char *, int, vfs_file *);
vfs_status vfs_close(vfs_file *);
vfs_status vfs_read(vfs_file *, void *, size_t, size_t *);
vfs_status vfs_write(vfs_file *, const void *, size_t, size_t *);
vfs_status vfs_remove(vfs *, const char *);
vfs_status vfs_rename(vfs *, const char *, const char *);
vfs_status vfs_sync(vfs *);
void vfs_destroy(vfs *);

typedef struct tmpfs_entry tmpfs_entry;
typedef struct tmpfs {
  vfs_memory memory;
  tmpfs_entry *entries;
  size_t count;
  size_t limit;
} tmpfs;
vfs_status tmpfs_init(tmpfs *, const vfs_memory *, size_t);
extern const vfs_ops tmpfs_operations;

#endif
