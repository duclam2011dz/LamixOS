/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef LAMIX_KERNEL_INITRAMFS_H
#define LAMIX_KERNEL_INITRAMFS_H

#include <stddef.h>
#include <stdint.h>

typedef enum lamix_initramfs_status {
  LAMIX_INITRAMFS_OK,
  LAMIX_INITRAMFS_INVALID,
  LAMIX_INITRAMFS_NOT_FOUND
} lamix_initramfs_status;

typedef struct lamix_initramfs_file {
  const uint8_t *data;
  size_t size;
} lamix_initramfs_file;

lamix_initramfs_status lamix_initramfs_find(const void *archive,
                                             size_t archive_size,
                                             const char *name,
                                             lamix_initramfs_file *file);

#endif
