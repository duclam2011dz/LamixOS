/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef LAMIX_KERNEL_VIRTIO_BLK_H
#define LAMIX_KERNEL_VIRTIO_BLK_H

#include "block.h"
#include <stddef.h>
#include <stdint.h>

#define VIRTIO_BLK_MAX_REQUESTS 8u
#define VIRTIO_BLK_QUEUE_SIZE 32u

typedef struct virtio_blk_desc {
  uint64_t address;
  uint32_t length;
  uint16_t flags;
  uint16_t next;
} __attribute__((packed)) virtio_blk_desc;

typedef struct virtio_blk_used_element {
  uint32_t id;
  uint32_t length;
} __attribute__((packed)) virtio_blk_used_element;

typedef struct virtio_blk_ops {
  void *context;
  uint32_t (*read_common)(void *context, uint16_t offset, unsigned width);
  void (*write_common)(void *context, uint16_t offset, unsigned width,
                       uint32_t value);
  uint32_t (*read_device)(void *context, uint16_t offset, unsigned width);
  void (*notify)(void *context, uint16_t queue, uint16_t offset);
  int (*dma_address)(void *context, const void *pointer, size_t length,
                     uint64_t *physical);
  void (*lock)(void *context);
  void (*unlock)(void *context);
  void (*wait_hint)(void *context);
} virtio_blk_ops;

typedef struct virtio_blk_memory {
  uint8_t *descriptors;
  size_t descriptors_size;
  uint64_t descriptors_physical;
  uint8_t *available;
  size_t available_size;
  uint64_t available_physical;
  uint8_t *used;
  size_t used_size;
  uint64_t used_physical;
  uint8_t *headers;
  size_t headers_size;
  uint8_t *statuses;
  size_t statuses_size;
} virtio_blk_memory;

typedef struct virtio_blk_slot {
  block_request *request;
  uint32_t bytes;
  uint16_t head;
  uint8_t operation;
  uint8_t active;
} virtio_blk_slot;

typedef struct virtio_blk {
  virtio_blk_ops operations;
  virtio_blk_memory memory;
  block_device device;
  virtio_blk_desc *descriptor_table;
  volatile uint16_t *available_flags;
  volatile uint16_t *available_index;
  volatile uint16_t *available_ring;
  volatile uint16_t *used_flags;
  volatile uint16_t *used_index;
  volatile virtio_blk_used_element *used_ring;
  virtio_blk_slot slots[VIRTIO_BLK_MAX_REQUESTS];
  uint16_t queue_size;
  uint16_t last_used;
  uint16_t notify_offset;
  uint32_t logical_block_size;
  uint8_t flush_supported;
  uint8_t initialized;
  uint8_t accepting;
  uint8_t offline;
} virtio_blk;

block_status virtio_blk_initialize(virtio_blk *driver,
                                   const virtio_blk_ops *operations,
                                   const virtio_blk_memory *memory);
size_t virtio_blk_poll(virtio_blk *driver, size_t budget);
block_status virtio_blk_reset(virtio_blk *driver);

#endif
