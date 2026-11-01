/* SPDX-License-Identifier: GPL-3.0-only */
#include "block.h"

#define BLOCK_REQUEST_IDLE 0u
#define BLOCK_REQUEST_ACTIVE 1u
#define BLOCK_REQUEST_DONE 2u
#define BLOCK_REQUEST_COMPLETING 3u

block_status block_device_init(block_device *device,
                               const block_device_ops *operations,
                               void *driver, uint64_t capacity_blocks,
                               uint32_t logical_block_size, int read_only) {
  if (!device || !operations || !operations->submit || !operations->poll ||
      !operations->reset || !driver || !capacity_blocks ||
      logical_block_size < 512 || logical_block_size > 65536 ||
      (logical_block_size & (logical_block_size - 1)) ||
      (logical_block_size % 512) || (read_only != 0 && read_only != 1))
    return BLOCK_INVALID;
  *device = (block_device){operations, driver, capacity_blocks,
                           logical_block_size, (uint8_t)read_only, 1};
  return BLOCK_OK;
}

void block_request_init(block_request *request, block_operation operation,
                        uint64_t lba, uint32_t block_count, void *buffer,
                        block_completion completion, void *context) {
  if (!request) return;
  *request = (block_request){operation, lba, block_count, buffer, BLOCK_BUSY,
                             completion, context, BLOCK_REQUEST_IDLE};
}

static block_status validate(block_device *device, block_request *request) {
  if (!device || !device->initialized || !request ||
      request->state != BLOCK_REQUEST_IDLE)
    return BLOCK_INVALID;
  if (request->operation < BLOCK_READ || request->operation > BLOCK_FLUSH)
    return BLOCK_INVALID;
  if (request->operation == BLOCK_FLUSH)
    return request->block_count == 0 && request->buffer == 0
               ? BLOCK_OK : BLOCK_INVALID;
  if (!request->block_count || !request->buffer ||
      request->lba >= device->capacity_blocks ||
      request->block_count > device->capacity_blocks - request->lba ||
      request->block_count > SIZE_MAX / device->logical_block_size)
    return BLOCK_RANGE;
  if (request->operation == BLOCK_WRITE && device->read_only)
    return BLOCK_READ_ONLY;
  return BLOCK_OK;
}

block_status block_submit(block_device *device, block_request *request) {
  block_status status = validate(device, request);
  if (status != BLOCK_OK) return status;
  request->status = BLOCK_BUSY;
  __atomic_store_n(&request->state, BLOCK_REQUEST_ACTIVE, __ATOMIC_RELEASE);
  status = device->operations->submit(device, request);
  if (status != BLOCK_OK) {
    request->status = status;
    __atomic_store_n(&request->state, BLOCK_REQUEST_IDLE, __ATOMIC_RELEASE);
  }
  return status;
}

size_t block_poll(block_device *device, size_t budget) {
  if (!device || !device->initialized || !budget) return 0;
  return device->operations->poll(device, budget);
}

block_status block_io_sync(block_device *device, block_request *request,
                           uint64_t poll_budget) {
  if (!poll_budget) return BLOCK_INVALID;
  block_status status = block_submit(device, request);
  if (status != BLOCK_OK) return status;
  while (poll_budget--) {
    if (__atomic_load_n(&request->state, __ATOMIC_ACQUIRE) ==
        BLOCK_REQUEST_DONE)
      return request->status;
    (void)block_poll(device, 16);
  }
  if (__atomic_load_n(&request->state, __ATOMIC_ACQUIRE) ==
      BLOCK_REQUEST_DONE)
    return request->status;
  status = device->operations->reset(device);
  if (status != BLOCK_OK) return BLOCK_RESET_TIMEOUT;
  uint32_t state = __atomic_load_n(&request->state, __ATOMIC_ACQUIRE);
  while (state == BLOCK_REQUEST_COMPLETING) {
    __asm__ volatile("pause" ::: "memory");
    state = __atomic_load_n(&request->state, __ATOMIC_ACQUIRE);
  }
  if (state == BLOCK_REQUEST_DONE) return request->status;
  return BLOCK_TIMEOUT;
}

void block_request_complete(block_request *request, block_status status) {
  if (!request) return;
  uint32_t expected = BLOCK_REQUEST_ACTIVE;
  if (!__atomic_compare_exchange_n(&request->state, &expected,
          BLOCK_REQUEST_COMPLETING, 0, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE))
    return;
  request->status = status;
  if (request->completion)
    request->completion(request, request->completion_context);
  __atomic_store_n(&request->state, BLOCK_REQUEST_DONE, __ATOMIC_RELEASE);
}
