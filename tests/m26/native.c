/* SPDX-License-Identifier: GPL-3.0-only */
#include "fat32.h"
#include "vfs.h"
#include <assert.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

typedef struct test_memory {
  pthread_mutex_t lock;
} test_memory;
static void *alloc(void *c, size_t n) {
  (void)c;
  return malloc(n);
}
static void dealloc(void *c, void *p) {
  (void)c;
  free(p);
}
static uintptr_t lock(void *c) {
  pthread_mutex_lock(&((test_memory *)c)->lock);
  return 0;
}
static void unlock(void *c, uintptr_t f) {
  (void)f;
  pthread_mutex_unlock(&((test_memory *)c)->lock);
}

#define FAT_TEST_SECTORS 66581u
typedef struct mock_disk {
  uint8_t *bytes;
  block_device device;
  uint32_t write_count;
  uint32_t fail_write_at;
} mock_disk;
static block_status disk_submit(block_device *d, block_request *r) {
  mock_disk *m = d->driver;
  uint64_t off = r->lba * 512;
  size_t n = (size_t)r->block_count * 512;
  if (r->operation == BLOCK_FLUSH) {
    block_request_complete(r, BLOCK_OK);
    return BLOCK_OK;
  }
  if (off > FAT_TEST_SECTORS * 512ull || n > FAT_TEST_SECTORS * 512ull - off)
    return BLOCK_RANGE;
  if (r->operation == BLOCK_READ)
    memcpy(r->buffer, m->bytes + off, n);
  else {
    ++m->write_count;
    if (m->fail_write_at && m->write_count == m->fail_write_at) {
      m->fail_write_at = 0;
      return BLOCK_DEVICE_ERROR;
    }
    memcpy(m->bytes + off, r->buffer, n);
  }
  block_request_complete(r, BLOCK_OK);
  return BLOCK_OK;
}
static size_t disk_poll(block_device *d, size_t n) {
  (void)d;
  (void)n;
  return 0;
}
static block_status disk_reset(block_device *d) {
  (void)d;
  return BLOCK_OK;
}
static void put16(uint8_t *p, uint16_t v) {
  p[0] = (uint8_t)v;
  p[1] = (uint8_t)(v >> 8);
}
static void put32(uint8_t *p, uint32_t v) {
  for (unsigned i = 0; i < 4; i++)
    p[i] = (uint8_t)(v >> (8 * i));
}
static void test_fat32(test_memory *tm, vfs_memory *mem) {
  static const block_device_ops ops = {disk_submit, disk_poll, disk_reset};
  mock_disk disk = {.bytes = calloc(FAT_TEST_SECTORS, 512)};
  assert(disk.bytes);
  assert(block_device_init(&disk.device, &ops, &disk, FAT_TEST_SECTORS, 512,
                           0) == BLOCK_OK);
  uint8_t *b = disk.bytes;
  b[0] = 0xeb;
  b[2] = 0x90;
  put16(b + 11, 512);
  b[13] = 1;
  put16(b + 14, 32);
  b[16] = 2;
  put32(b + 32, FAT_TEST_SECTORS);
  put32(b + 36, 512);
  put16(b + 40, 0);
  put32(b + 44, 2);
  put16(b + 48, 1);
  put16(b + 50, 6);
  b[510] = 0x55;
  b[511] = 0xaa;
  memcpy(disk.bytes + 6 * 512, b, 512);
  uint32_t clusters = FAT_TEST_SECTORS - 32 - 1024;
  assert(clusters >= 65525);
  put32(disk.bytes + 32 * 512 + 8, 0x0fffffff);
  put32(disk.bytes + (32 + 512) * 512 + 8, 0x0fffffff);
  uint8_t *fsinfo = disk.bytes + 512;
  put32(fsinfo, 0x41615252);
  put32(fsinfo + 484, 0x61417272);
  put32(fsinfo + 508, 0xaa550000);
  fat32 fat;
  assert(fat32_mount(&fat, &disk.device, 0, FAT_TEST_SECTORS, mem) == VFS_OK);
  tmpfs root;
  vfs fs;
  vfs_mount rm, fm;
  vfs_node node;
  assert(tmpfs_init(&root, mem, 8) == VFS_OK);
  assert(vfs_init(&fs, mem) == VFS_OK);
  assert(vfs_mount_fs(&fs, "/", &tmpfs_operations, &root, &rm) == VFS_OK);
  assert(vfs_mount_fs(&fs, "/data", &fat32_operations, &fat, &fm) == VFS_OK);
  assert(vfs_create(&fs, "/data/Long filename.txt", VFS_FILE) == VFS_OK);
  vfs_file f;
  assert(vfs_open(&fs, "/data/Long filename.txt", 1, &f) == VFS_OK);
  const char text[] = "persistent FAT32 payload";
  size_t n;
  assert(vfs_write(&f, text, sizeof(text), &n) == VFS_OK && n == sizeof(text));
  assert(vfs_close(&f) == VFS_OK);
  assert(vfs_open(&fs, "/data/Long filename.txt", 0, &f) == VFS_OK);
  char out[sizeof(text)];
  assert(vfs_read(&f, out, sizeof(out), &n) == VFS_OK && n == sizeof(out) &&
         !memcmp(out, text, n));
  assert(vfs_close(&f) == VFS_OK);
  assert(vfs_rename(&fs, "/data/Long filename.txt", "/data/Renamed file.txt") ==
         VFS_OK);
  assert(vfs_remove(&fs, "/data/Renamed file.txt") == VFS_OK);
  assert(vfs_create(&fs, "/data/Docs", VFS_DIRECTORY) == VFS_OK);
  assert(vfs_create(&fs, "/data/Docs/Nested long file.txt", VFS_FILE) ==
         VFS_OK);
  assert(vfs_remove(&fs, "/data/Docs") == VFS_BUSY);
  assert(vfs_open(&fs, "/data/Docs/Nested long file.txt", 1, &f) == VFS_OK);
  uint8_t nested_payload[1024];
  for (size_t i = 0; i < sizeof(nested_payload); ++i)
    nested_payload[i] = (uint8_t)(i ^ 0xa7);
  assert(vfs_write(&f, nested_payload, sizeof(nested_payload), &n) == VFS_OK &&
         n == sizeof(nested_payload));
  assert(vfs_close(&f) == VFS_OK);
  assert(vfs_open(&fs, "/data/Docs/Nested long file.txt", 0, &f) == VFS_OK);
  uint8_t nested_readback[sizeof(nested_payload)];
  assert(vfs_read(&f, nested_readback, sizeof(nested_readback), &n) == VFS_OK &&
         n == sizeof(nested_readback) &&
         !memcmp(nested_readback, nested_payload, sizeof(nested_payload)));
  assert(vfs_close(&f) == VFS_OK);
  assert(vfs_remove(&fs, "/data/Docs/Nested long file.txt") == VFS_OK);
  assert(vfs_remove(&fs, "/data/Docs") == VFS_OK);
  assert(vfs_create(&fs, "/data/ROLLBACK.BIN", VFS_FILE) == VFS_OK);
  assert(vfs_open(&fs, "/data/ROLLBACK.BIN", 1, &f) == VFS_OK);
  uint8_t rollback_payload[1024] = {0x5c};
  disk.fail_write_at = disk.write_count + 10;
  assert(vfs_write(&f, rollback_payload, sizeof(rollback_payload), &n) ==
         VFS_IO_ERROR);
  assert(vfs_close(&f) == VFS_OK);
  assert(vfs_lookup(&fs, "/data/ROLLBACK.BIN", &node) == VFS_OK &&
         node.size == 0);
  assert(vfs_remove(&fs, "/data/ROLLBACK.BIN") == VFS_OK);
  assert(vfs_create(&fs, "/data/Checksummed long.txt", VFS_FILE) == VFS_OK);
  assert(vfs_create(&fs, "/data/Checksummed other.txt", VFS_FILE) == VFS_OK);
  uint8_t *root_entries = disk.bytes + (32 + 2 * 512) * 512ull;
  static const uint8_t checksum_alias[11] = "CHECKS~1TXT";
  static const uint8_t collision_alias[11] = "CHECKS~2TXT";
  int short_slot = -1;
  int collision_slot = -1;
  for (int i = 0; i < 16; ++i)
    if (!memcmp(root_entries + i * 32, checksum_alias, 11))
      short_slot = i;
    else if (!memcmp(root_entries + i * 32, collision_alias, 11))
      collision_slot = i;
  assert(collision_slot > short_slot);
  assert(short_slot > 0 && root_entries[(short_slot - 1) * 32 + 11] == 0x0f);
  root_entries[(short_slot - 1) * 32 + 13] ^= 0x80;
  assert(vfs_lookup(&fs, "/data/Checksummed long.txt", &node) == VFS_NOT_FOUND);
  assert(vfs_lookup(&fs, "/data/CHECKS~1.TXT", &node) == VFS_OK);
  assert(vfs_remove(&fs, "/data/CHECKS~1.TXT") == VFS_OK);
  assert(vfs_lookup(&fs, "/data/CHECKS~2.TXT", &node) == VFS_OK);
  assert(vfs_remove(&fs, "/data/CHECKS~2.TXT") == VFS_OK);

  assert(vfs_create(&fs, "/data/BROKEN.BIN", VFS_FILE) == VFS_OK);
  assert(vfs_open(&fs, "/data/BROKEN.BIN", 1, &f) == VFS_OK);
  const uint8_t byte = 0x5a;
  assert(vfs_write(&f, &byte, 1, &n) == VFS_OK && n == 1);
  assert(vfs_close(&f) == VFS_OK);
  static const uint8_t broken_alias[11] = "BROKEN  BIN";
  int broken_slot = -1;
  for (int i = 0; i < 16; ++i)
    if (!memcmp(root_entries + i * 32, broken_alias, 11))
      broken_slot = i;
  assert(broken_slot >= 0);
  uint8_t *broken_entry = root_entries + broken_slot * 32;
  uint32_t broken_cluster = (uint32_t)(broken_entry[26] |
      ((uint32_t)broken_entry[27] << 8) |
      ((uint32_t)broken_entry[20] << 16) |
      ((uint32_t)broken_entry[21] << 24));
  assert(broken_cluster >= 2);
  uint32_t fat_offset = 32 * 512 + broken_cluster * 4;
  put32(disk.bytes + fat_offset, 0x0ffffff7);
  put32(disk.bytes + fat_offset + 512 * 512, 0x0ffffff7);
  assert(vfs_open(&fs, "/data/BROKEN.BIN", 0, &f) == VFS_OK);
  uint8_t broken_read;
  assert(vfs_read(&f, &broken_read, 1, &n) == VFS_CORRUPT && n == 0);
  assert(vfs_close(&f) == VFS_OK);
  assert(vfs_remove(&fs, "/data/BROKEN.BIN") == VFS_CORRUPT);
  assert(vfs_lookup(&fs, "/data/BROKEN.BIN", &node) == VFS_OK);
  assert(vfs_sync(&fs) == VFS_OK);
  vfs_destroy(&fs);
  put16(b + 11, 1024);
  assert(fat32_mount(&fat, &disk.device, 0, FAT_TEST_SECTORS, mem) ==
         VFS_CORRUPT);
  free(disk.bytes);
  (void)tm;
}

int main(void) {
  test_memory tm;
  assert(!pthread_mutex_init(&tm.lock, 0));
  vfs_memory mem = {&tm, alloc, dealloc, lock, unlock};
  tmpfs root, data;
  vfs fs;
  vfs_mount root_mount, data_mount;
  assert(tmpfs_init(&root, &mem, 64) == VFS_OK);
  assert(tmpfs_init(&data, &mem, 64) == VFS_OK);
  assert(vfs_init(&fs, &mem) == VFS_OK);
  assert(vfs_mount_fs(&fs, "/", &tmpfs_operations, &root, &root_mount) ==
         VFS_OK);
  assert(vfs_mount_fs(&fs, "/data", &tmpfs_operations, &data, &data_mount) ==
         VFS_OK);
  assert(vfs_create(&fs, "/data/file.txt", VFS_FILE) == VFS_OK);
  vfs_file f;
  assert(vfs_open(&fs, "/data/file.txt", 1, &f) == VFS_OK);
  static const char payload[] = "Lamix VFS M26";
  size_t done = 0;
  assert(vfs_write(&f, payload, sizeof(payload), &done) == VFS_OK &&
         done == sizeof(payload));
  assert(vfs_unmount(&fs, "/data") == VFS_BUSY);
  assert(vfs_close(&f) == VFS_OK);
  assert(vfs_rename(&fs, "/data/file.txt", "/data/renamed long name.txt") ==
         VFS_OK);
  assert(vfs_open(&fs, "/data/renamed long name.txt", 0, &f) == VFS_OK);
  char output[sizeof(payload)] = {0};
  assert(vfs_read(&f, output, sizeof(output), &done) == VFS_OK &&
         done == sizeof(output));
  assert(!memcmp(output, payload, sizeof(payload)));
  assert(vfs_close(&f) == VFS_OK);
  assert(vfs_remove(&fs, "/data/renamed long name.txt") == VFS_OK);
  assert(vfs_lookup(&fs, "/data/renamed long name.txt", &(vfs_node){0}) ==
         VFS_NOT_FOUND);
  assert(vfs_create(&fs, "/tmp.txt", VFS_FILE) == VFS_OK);
  assert(vfs_open(&fs, "/tmp.txt", 1, &f) == VFS_OK);
  assert(vfs_write(&f, payload, sizeof(payload), &done) == VFS_OK);
  assert(vfs_close(&f) == VFS_OK);
  assert(vfs_unmount(&fs, "/data") == VFS_OK);
  assert(vfs_lookup(&fs, "/tmp.txt", &(vfs_node){0}) == VFS_OK);
  vfs_destroy(&fs);
  test_fat32(&tm, &mem);
  assert(!pthread_mutex_destroy(&tm.lock));
  return 0;
}
