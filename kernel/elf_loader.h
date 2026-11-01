/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef LAMIX_KERNEL_ELF_LOADER_H
#define LAMIX_KERNEL_ELF_LOADER_H

#include <stddef.h>
#include <stdint.h>

#define LAMIX_ELF_MAX_SEGMENTS 16u
#define LAMIX_ELF_LOAD_BASE UINT64_C(0x0000008000000000)
#define LAMIX_ELF_USER_LIMIT UINT64_C(0x0000010000000000)
#define LAMIX_ELF_MAX_SPAN UINT64_C(0x00200000)

typedef enum lamix_elf_status {
  LAMIX_ELF_OK,
  LAMIX_ELF_INVALID,
  LAMIX_ELF_UNSUPPORTED,
  LAMIX_ELF_NO_MEMORY
} lamix_elf_status;

typedef struct lamix_elf_allocator {
  void *context;
  void *(*allocate)(void *context, size_t size);
  void (*deallocate)(void *context, void *memory);
} lamix_elf_allocator;

typedef struct lamix_elf_segment {
  uint64_t address;
  uint64_t offset;
  uint64_t file_size;
  uint64_t memory_size;
  uint32_t flags;
} lamix_elf_segment;

typedef struct lamix_elf_image {
  lamix_elf_allocator allocator;
  uint8_t *bytes;
  size_t size;
  uint64_t base;
  uint64_t entry;
  uint64_t minimum_vaddr;
  uint32_t segment_count;
  lamix_elf_segment segments[LAMIX_ELF_MAX_SEGMENTS];
} lamix_elf_image;

lamix_elf_status lamix_elf_prepare(const void *file, size_t file_size,
                                    uint64_t load_base,
                                    const lamix_elf_allocator *allocator,
                                    lamix_elf_image *image);
void lamix_elf_release(lamix_elf_image *image);

#endif
