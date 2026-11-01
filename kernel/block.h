/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef LAMIX_KERNEL_BLOCK_H
#define LAMIX_KERNEL_BLOCK_H

#include <stddef.h>
#include <stdint.h>

typedef enum block_status {
  BLOCK_OK = 0,
  BLOCK_INVALID,
  BLOCK_RANGE,
  BLOCK_READ_ONLY,
  BLOCK_UNSUPPORTED,
  BLOCK_DEVICE_ERROR,
  BLOCK_TIMEOUT,
  BLOCK_RESET_TIMEOUT,
  BLOCK_BUSY,
  BLOCK_NO_MEMORY
} block_status;

typedef enum block_operation {
  BLOCK_READ = 0,
  BLOCK_WRITE,
  BLOCK_FLUSH
} block_operation;

typedef struct block_request block_request;
typedef void (*block_completion)(block_request *request, void *context);

struct block_request {
  block_operation operation;
  uint64_t lba;
  uint32_t block_count;
  void *buffer;
  block_status status;
  block_completion completion;
  void *completion_context;
  volatile uint32_t state;
};

/* Request and buffer stay live through callback return; DONE is published after. */

struct block_device;
typedef struct block_device_ops {
  block_status (*submit)(struct block_device *device, block_request *request);
  size_t (*poll)(struct block_device *device, size_t budget);
  block_status (*reset)(struct block_device *device);
} block_device_ops;

typedef struct block_device {
  const block_device_ops *operations;
  void *driver;
  uint64_t capacity_blocks;
  uint32_t logical_block_size;
  uint8_t read_only;
  uint8_t initialized;
} block_device;

block_status block_device_init(block_device *device,
                               const block_device_ops *operations,
                               void *driver, uint64_t capacity_blocks,
                               uint32_t logical_block_size, int read_only);
void block_request_init(block_request *request, block_operation operation,
                        uint64_t lba, uint32_t block_count, void *buffer,
                        block_completion completion, void *context);
block_status block_submit(block_device *device, block_request *request);
size_t block_poll(block_device *device, size_t budget);
block_status block_io_sync(block_device *device, block_request *request,
                           uint64_t poll_budget);
void block_request_complete(block_request *request, block_status status);

#endif
