/* SPDX-License-Identifier: GPL-3.0-only */
#define _FILE_OFFSET_BITS 64
#include "lamix/boot/loader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
extern uint64_t lamix_divmod64(uint64_t n, uint64_t d, uint64_t *r);
static int read_disk(void *context, uint64_t off, void *bytes, uint32_t size) {
  FILE *f = context;
  return off <= INT64_MAX && fseeko(f, (off_t)off, SEEK_SET) == 0 &&
         fread(bytes, 1, size, f) == size;
}
static int read_buffer(void *context, uint64_t off, void *bytes,
                       uint32_t size) {
  lamix_reader *r = context;
  if (off > r->size || size > r->size - off)
    return 0;
  memcpy(bytes, (const uint8_t *)r->context + off, size);
  return 1;
}
typedef struct host_allocations {
  void *pages[32];
  uint32_t count;
} host_allocations;
static int host_allocate(void *context, uint32_t size, uint64_t *physical,
                         void **mapped) {
  host_allocations *a = context;
  if (a->count == 32)
    return 0;
  void *p = malloc(size);
  if (!p)
    return 0;
  a->pages[a->count++] = p;
  *mapped = p;
  *physical = 0x100000 + (uint64_t)a->count * 0x4000000;
  return 1;
}
int main(int argc, char **argv) {
  uint64_t state = UINT64_C(0x123456789abcdef0);
  for (unsigned i = 0; i < 10000; i++) {
    state ^= state << 13;
    state ^= state >> 7;
    state ^= state << 17;
    uint64_t n = state,
             d = (i % 64 == 0 ? UINT64_MAX
                              : ((state >> 5) ^ UINT64_C(0xfefefefefefefefe))) |
                 1,
             r;
    if (lamix_divmod64(n, d, &r) != n / d || r != n % d)
      return 1;
  }
  if (argc < 3 || argc % 2 != 1)
    return 2;
  for (int i = 1; i < argc; i += 2) {
    FILE *f = fopen(argv[i], "rb");
    if (!f)
      return 3;
    if (fseeko(f, 0, SEEK_END)) {
      fclose(f);
      return 4;
    }
    off_t size = ftello(f);
    if (size < 0) {
      fclose(f);
      return 5;
    }
    lamix_reader reader = {f, read_disk, (uint64_t)size};
    lamix_disk disk;
    lamix_fat fat;
    lamix_elf_plan plan;
    uint8_t array[16384];
    const char *stage = "GPT";
    if (lamix_gpt_open(reader, 2048, 6143, array, sizeof array, &disk) ==
        LBP_OK) {
      stage = "FAT";
      if (lamix_fat_open(&disk, &fat) == LBP_OK) {
        stage = "ELF";
        lamix_reader file = {&fat, lamix_fat_read, fat.file_size};
        if (lamix_elf_inspect(file, 4, &plan) == LBP_OK) {
          stage = "OK";
          host_allocations allocations = {0};
          lbp_segment loaded[32];
          lbp_status loaded_status = lamix_elf_load(file, &plan, host_allocate,
                                                    &allocations, loaded, 32);
          int zero_ok = 1;
          if (loaded_status == LBP_OK)
            for (uint32_t segment = 0; segment < plan.count; segment++) {
              const uint8_t *data = allocations.pages[segment];
              for (uint64_t at = plan.segments[segment].file_size;
                   at < loaded[segment].length; at++)
                if (data[at])
                  zero_ok = 0;
            }
          for (uint32_t a = 0; a < allocations.count; a++)
            free(allocations.pages[a]);
          if (loaded_status != LBP_OK || !zero_ok) {
            fclose(f);
            return 10;
          }
          uint8_t *image = malloc(fat.file_size);
          if (!image)
            return 6;
          if (!lamix_fat_read(&fat, 0, image, fat.file_size)) {
            free(image);
            fclose(f);
            return 7;
          }
          lamix_reader buffer = {image, 0, fat.file_size};
          lamix_reader b = {&buffer, read_buffer, fat.file_size};
          for (uint32_t at = 0; at < 64; at++) {
            uint8_t saved = image[at];
            image[at] ^= 0xff;
            lamix_elf_plan output;
            memset(&output, 0xa5, sizeof output);
            lamix_elf_plan unchanged = output;
            lbp_status s = lamix_elf_inspect(b, 4, &output);
            if (s && memcmp(&output, &unchanged, sizeof output)) {
              free(image);
              fclose(f);
              return 8;
            }
            image[at] = saved;
          }
          for (uint64_t n = 0; n < 288; n++) {
            b.size = n;
            (void)lamix_elf_inspect(b, 4, &plan);
          }
          free(image);
        }
      }
    }
    fclose(f);
    if (strcmp(stage, argv[i + 1])) {
      fprintf(stderr, "expected %s, got %s: %s\n", argv[i + 1], stage, argv[i]);
      return 9;
    }
  }
  puts("PASS: host native parsers/division/mutations; không boot");
  return 0;
}
